# Original project brief

> I am working on an engineering project that takes a newer PL60C that has a
> wired 5-pin DMX connector on it and turns that into a wake-up lamp. The
> goal, therefore, is to use this port to ramp up the light in the morning.
>
> Now, I can be your hands in the world, but I really want you to act very,
> very autonomously throughout this, really just pursuing what you need to.
> The goal for me is to have some electronics to control this DMX port and to
> ramp up the lamp in the morning. My basic idea is to have an ESP32 and use
> that to somehow control this port. You need to work out the protocol and
> all of this and how to wire it in. Ideally, what I want you to make is
> actual designs for the hardware, for example, a printed circuit board
> design.
>
> You're going to loop and loop and loop and loop and loop until you really
> think you've actually completed this, an actual finished design. At the
> same time, you should be thinking about the firmware that runs on the ESP32
> and presents on my home Wi-Fi a portal that I can log in to. Clarifying
> questions: ask me those questions, and then, once you've done that, you're
> going to go and run as a really great electronic product engineer who's
> making this beautiful MVP product. You're going to give me parts lists,
> you're going to give me made-up PCBs, all sorts, until you get to the point
> where I can just click the buttons to order and I can have a wake lamp that
> lights up just the next morning.

— Tom Milton, 10 June 2026

## Clarifications given

- Lamp: **Neewer PL60C** LED panel
- Build path: **both** — module build for speed + custom PCB as the product
- Region: **UK** sourcing
- Portal: sunrise CCT ramp, per-day schedules, manual control page, NTP with
  automatic DST
