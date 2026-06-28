#include "discovery.h"
#include "config.h"
#include <WiFi.h>
#include "mdns.h"

static char g_default[24];        // "wakelight-XXXX" (per-lamp fallback, unique)
static char g_id[8];              // MAC-derived election id, e.g. "a1b2c3"
static char g_slug[33];
static char g_chosen[33] = "wakelight";
static bool g_holds = false;      // do we currently own the wakelight.local delegate?

static void slugify(const char* in, char* out, size_t cap) {
  size_t o = 0; bool prevDash = false;
  for (const char* p = in; *p && o + 1 < cap; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out[o++] = c; prevDash = false; }
    else if ((c == ' ' || c == '-' || c == '_') && o > 0 && !prevDash) { out[o++] = '-'; prevDash = true; }
  }
  while (o > 0 && out[o - 1] == '-') o--;
  out[o] = 0;
}

static void computeIds() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(g_default, sizeof(g_default), "wakelight-%02x%02x", mac[4], mac[5]);
  snprintf(g_id, sizeof(g_id), "%02x%02x%02x", mac[3], mac[4], mac[5]);
}

static void recomputeSlug() {
  char s[33]; slugify(cfg.name, s, sizeof(s));
  strlcpy(g_slug, s[0] ? s : g_default, sizeof(g_slug));
}

static bool hostnameTaken(const char* host) {
  esp_ip4_addr_t tmp;
  return mdns_query_a(host, 250, &tmp) == ESP_OK;
}

// Per-lamp hostname. Never literally "wakelight" — that name is reserved for the
// shared delegate (see the election below), so a lamp address can't collide with it.
static void pickHostname(char* out, size_t cap) {
  const char* base = (strcmp(g_slug, "wakelight") == 0) ? g_default : g_slug;
  if (!hostnameTaken(base)) { strlcpy(out, base, cap); return; }
  for (int n = 2; n <= 9; n++) {
    char cand[40]; snprintf(cand, sizeof(cand), "%s-%d", base, n);
    if (!hostnameTaken(cand)) { strlcpy(out, cand, cap); return; }
  }
  strlcpy(out, g_default, cap);
}

static void updateTxt() {
  mdns_txt_item_t items[] = {
    {"name", (char*)cfg.name}, {"slug", g_slug}, {"host", g_chosen}, {"id", g_id}
  };
  mdns_service_txt_set("_wakelight", "_tcp", items, 4);
}

void Discovery::applyIdentity() {
  recomputeSlug();
  char chosen[33]; pickHostname(chosen, sizeof(chosen));
  mdns_hostname_set(chosen);
  mdns_instance_name_set(cfg.name);
  strlcpy(g_chosen, chosen, sizeof(g_chosen));
  updateTxt();   // no-op until the service exists (boot calls it again)
}

static const char* txtLookup(const mdns_result_t* r, const char* key) {
  for (size_t i = 0; i < r->txt_count; i++)
    if (r->txt[i].key && strcmp(r->txt[i].key, key) == 0) return r->txt[i].value;
  return nullptr;
}

// ---- conflict-safe wakelight.local: the lowest-id live lamp owns it ----
static void setOwner(bool own) {
  if (own && !g_holds) {
    uint32_t v4 = (uint32_t)WiFi.localIP();
    if (!v4) return;
    mdns_ip_addr_t ip; memset(&ip, 0, sizeof(ip));
    ip.addr.type = ESP_IPADDR_TYPE_V4;
    ip.addr.u_addr.ip4.addr = v4;
    ip.next = nullptr;
    if (mdns_delegate_hostname_add("wakelight", &ip) == ESP_OK) g_holds = true;
  } else if (!own && g_holds) {
    mdns_delegate_hostname_remove("wakelight");
    g_holds = false;
  }
}

// Decide ownership from who is actually online: lowest id wins. Deterministic,
// so two lamps that can see each other never both claim it; self-healing, so if
// the owner drops off the next-lowest takes over on the following tick.
static void runElection() {
  char minId[8]; strlcpy(minId, g_id, sizeof(minId));
  mdns_result_t* res = nullptr;
  mdns_query_ptr("_wakelight", "_tcp", 2000, 16, &res);
  for (mdns_result_t* r = res; r; r = r->next) {
    const char* host = txtLookup(r, "host");
    if (host && strcmp(host, g_chosen) == 0) continue;   // skip self
    const char* id = txtLookup(r, "id");
    if (id && id[0] && strcmp(id, minId) < 0) strlcpy(minId, id, sizeof(minId));
  }
  if (res) mdns_query_results_free(res);
  setOwner(strcmp(g_id, minId) == 0);
}

static void electionTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(6000));            // let discovery settle after boot
  for (;;) { runElection(); vTaskDelay(pdMS_TO_TICKS(15000)); }
}

void Discovery::begin() {
  computeIds();
  if (mdns_init() != ESP_OK) return;
  applyIdentity();
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  mdns_service_add(nullptr, "_wakelight", "_tcp", 80, nullptr, 0);
  updateTxt();
  xTaskCreate(electionTask, "wl-elect", 4096, nullptr, 1, nullptr);
}

const char* Discovery::defaultHost() { return g_default; }
const char* Discovery::chosenHost()  { return g_chosen; }
const char* Discovery::slug()        { return g_slug; }
bool Discovery::holdsWakelight()     { return g_holds; }

void Discovery::buildPeers(JsonDocument& doc) {
  JsonArray arr = doc["peers"].to<JsonArray>();
  JsonObject me = arr.add<JsonObject>();
  me["name"] = cfg.name; me["host"] = g_chosen; me["slug"] = g_slug; me["self"] = true;

  String seen = String("|") + g_chosen + "|";   // skip-self + dedup by unique host
  // A cold one-shot PTR query often misses peers that the OS browser (which
  // listens continuously) sees, so use a longer window and retry once if empty.
  mdns_result_t* results = nullptr;
  mdns_query_ptr("_wakelight", "_tcp", 2500, 16, &results);
  if (!results) mdns_query_ptr("_wakelight", "_tcp", 2500, 16, &results);
  if (results) {
    for (mdns_result_t* r = results; r; r = r->next) {
      const char* host = txtLookup(r, "host");
      const char* slug = txtLookup(r, "slug");
      const char* name = txtLookup(r, "name");
      const char* id = (host && host[0]) ? host : slug;   // host preferred; slug for old peers
      if (!id || !id[0]) continue;
      String key = String("|") + id + "|";
      if (seen.indexOf(key) >= 0) continue;               // self or duplicate
      seen += id; seen += "|";
      JsonObject p = arr.add<JsonObject>();
      p["name"] = (name && name[0]) ? name : id;
      p["host"] = (host && host[0]) ? host : (slug ? slug : id);
      p["slug"] = slug ? slug : id;
      p["self"] = false;
    }
    mdns_query_results_free(results);
  }
}
