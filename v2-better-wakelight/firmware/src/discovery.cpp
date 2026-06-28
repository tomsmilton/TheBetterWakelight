#include "discovery.h"
#include "config.h"
#include <WiFi.h>
#include "mdns.h"

static char g_default[24];        // "wakelight-XXXX"
static char g_slug[33];
static char g_chosen[33] = "wakelight";
static bool g_holds = false;

static void slugify(const char* in, char* out, size_t cap) {
  size_t o = 0; bool prevDash = false;
  for (const char* p = in; *p && o + 1 < cap; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out[o++] = c; prevDash = false; }
    else if ((c == ' ' || c == '-' || c == '_') && o > 0 && !prevDash) { out[o++] = '-'; prevDash = true; }
  }
  while (o > 0 && out[o - 1] == '-') o--;   // trim trailing dash
  out[o] = 0;
}

static void computeDefault() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(g_default, sizeof(g_default), "wakelight-%02x%02x", mac[4], mac[5]);
}

static void recomputeSlug() {
  char s[33]; slugify(cfg.name, s, sizeof(s));
  strlcpy(g_slug, s[0] ? s : g_default, sizeof(g_slug));
}

// 250 ms is enough for same-LAN mDNS; longer just slows boot/rename.
static bool hostnameTaken(const char* host) {
  esp_ip4_addr_t tmp;
  return mdns_query_a(host, 250, &tmp) == ESP_OK;
}

// slug -> slug-2 .. slug-9 -> wakelight-XXXX (guaranteed unique)
static void pickHostname(char* out, size_t cap) {
  if (!hostnameTaken(g_slug)) { strlcpy(out, g_slug, cap); return; }
  for (int n = 2; n <= 9; n++) {
    char cand[40]; snprintf(cand, sizeof(cand), "%s-%d", g_slug, n);
    if (!hostnameTaken(cand)) { strlcpy(out, cand, cap); return; }
  }
  strlcpy(out, g_default, cap);
}

static bool myIp4(uint32_t* out) {
  uint32_t v = (uint32_t)WiFi.localIP();
  if (!v) return false; *out = v; return true;
}

// If wakelight.local is free (or we are it), claim it as a delegate so any
// device lands on a lamp that can list its peers.
static void tryClaimWakelight() {
  if (strcmp(g_chosen, "wakelight") == 0) { g_holds = true; return; }
  if (g_holds) return;
  if (hostnameTaken("wakelight")) return;
  uint32_t v4; if (!myIp4(&v4)) return;
  mdns_ip_addr_t ip; memset(&ip, 0, sizeof(ip));
  ip.addr.type = ESP_IPADDR_TYPE_V4;
  ip.addr.u_addr.ip4.addr = v4;
  ip.next = nullptr;
  if (mdns_delegate_hostname_add("wakelight", &ip) == ESP_OK) g_holds = true;
}

static void updateTxt() {
  mdns_txt_item_t items[] = { {"name", (char*)cfg.name}, {"slug", g_slug} };
  mdns_service_txt_set("_wakelight", "_tcp", items, 2);
}

void Discovery::applyIdentity() {
  recomputeSlug();
  char chosen[33]; pickHostname(chosen, sizeof(chosen));
  mdns_hostname_set(chosen);
  mdns_instance_name_set(cfg.name);
  strlcpy(g_chosen, chosen, sizeof(g_chosen));
  tryClaimWakelight();
  updateTxt();   // no-op if the service isn't added yet (boot calls it again)
}

void Discovery::begin() {
  computeDefault();
  if (mdns_init() != ESP_OK) return;
  recomputeSlug();
  char chosen[33]; pickHostname(chosen, sizeof(chosen));
  mdns_hostname_set(chosen);
  mdns_instance_name_set(cfg.name);
  strlcpy(g_chosen, chosen, sizeof(g_chosen));
  tryClaimWakelight();
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  mdns_service_add(nullptr, "_wakelight", "_tcp", 80, nullptr, 0);
  updateTxt();
}

const char* Discovery::defaultHost() { return g_default; }
const char* Discovery::chosenHost()  { return g_chosen; }
const char* Discovery::slug()        { return g_slug; }
bool Discovery::holdsWakelight()     { return g_holds; }

static const char* txtLookup(const mdns_result_t* r, const char* key) {
  for (size_t i = 0; i < r->txt_count; i++)
    if (r->txt[i].key && strcmp(r->txt[i].key, key) == 0) return r->txt[i].value;
  return nullptr;
}

void Discovery::buildPeers(JsonDocument& doc) {
  JsonArray arr = doc["peers"].to<JsonArray>();
  JsonObject me = arr.add<JsonObject>();
  me["name"] = cfg.name; me["slug"] = g_slug; me["self"] = true;

  mdns_result_t* results = nullptr;
  if (mdns_query_ptr("_wakelight", "_tcp", 1000, 8, &results) == ESP_OK) {
    for (mdns_result_t* r = results; r; r = r->next) {
      const char* slug = txtLookup(r, "slug");
      if (!slug || !slug[0] || strcmp(slug, g_slug) == 0) continue;  // skip self / unnamed
      const char* name = txtLookup(r, "name");
      JsonObject p = arr.add<JsonObject>();
      p["name"] = (name && name[0]) ? name : slug;
      p["slug"] = slug; p["self"] = false;
    }
    mdns_query_results_free(results);
  }
}
