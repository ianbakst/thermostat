#pragma once
//
// The page at /: the unit's controls. Everything on it that changes arrives
// over the control API (net/direct_api.h) once it's loaded, so this is sent
// as it is, after the shared start of the page.
//
// It behaves like the panel: the same words for each setting
// (panel/src/climate/climate.cpp), a background that follows the mode and
// whether the unit is running, and no setpoint in Off, Dry or Fan, where the
// unit doesn't use one.
//
// A tap shows its value straight away, dimmed, until the unit reports it
// back. The setpoint is sent only once the tapping stops, because each change
// ties up the link to the unit for a second or two. A value the unit never
// reports back (it can't set exactly that, or the change was lost) is dropped
// after a while, and the page shows what the unit does report.
//

inline constexpr const char* CONTROL_PAGE_STYLE =
    "body{transition:background .4s}"
    ".setpoint{display:flex;align-items:center;justify-content:space-between;margin:28px 0 4px}"
    ".setpoint button{width:64px;height:64px;margin:0;padding:0;border-radius:50%;font-size:30px;"
    "background:rgba(255,255,255,.15)}"
    "#target{font-size:56px;font-weight:300;text-align:center}#room{text-align:center}"
    ".choices{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}"
    ".choices button{width:auto;flex:1 0 auto;margin:0;padding:10px 12px;font-size:15px;"
    "background:rgba(255,255,255,.12)}"
    ".choices button.on{background:#eee;color:#111}"
    ".waiting{opacity:.55}button:disabled{opacity:.35}#error{color:#ffab91}";

inline constexpr const char* CONTROL_PAGE_BODY = R"html(
<p class=sub id=status>Connecting…</p>
<div class=setpoint>
  <button id=down aria-label=Lower>&minus;</button>
  <div><div id=target>--</div><p class=sub id=room></p></div>
  <button id=up aria-label=Raise>+</button>
</div>
<p class=hint id=error></p>
<h2>Mode</h2><div class=choices id=mode></div>
<h2>Fan</h2><div class=choices id=fan></div>
<h2>Vertical vane</h2><div class=choices id=vane></div>
<p class=hint>1 is the flattest, 5 points straight down.</p>
<h2>Horizontal vane</h2><div class=choices id=wideVane></div>
<p class=sub style="margin-top:28px"><a href=/settings>Status and settings</a></p>
<script>
// The protocol's words, and what the panel calls them.
const CHOICES = {
  mode: [["off", "Off"], ["heat", "Heat"], ["cool", "Cool"], ["heat_cool", "Auto"],
         ["dry", "Dry"], ["fan_only", "Fan"]],
  fan: [["auto", "Auto"], ["diffuse", "Quiet"], ["low", "Low"], ["middle", "Middle"],
        ["medium", "Medium"], ["high", "High"]],
  vane: [["AUTO", "Auto"], ["1", "1"], ["2", "2"], ["3", "3"], ["4", "4"], ["5", "5"],
         ["SWING", "Swing"]],
  wideVane: [["<<", "Far left"], ["<", "Left"], ["|", "Center"], [">", "Right"],
             [">>", "Far right"], ["<>", "Spread"], ["SWING", "Swing"]],
};
const ACTIONS = {off: "Off", idle: "Idle", heating: "Heating", cooling: "Cooling",
                 drying: "Drying", fan: "Fan"};
const PENDING_MS = 10000;  // how long to wait for the unit to report a change back

const $ = id => document.getElementById(id);
let ws, state = {}, pending = {}, setpointTimer;

// The value just asked for until the unit reports it, otherwise the unit's.
const shown = key => key in pending ? pending[key].value : state[key];

function ask(change) {
  for (const key in change) pending[key] = {value: change[key], at: Date.now()};
  $("error").textContent = "";
  render();
  if (ws && ws.readyState == WebSocket.OPEN) ws.send(JSON.stringify(change));
}

for (const key in CHOICES) {
  for (const [value, label] of CHOICES[key]) {
    const button = document.createElement("button");
    button.textContent = label;
    button.dataset.value = value;
    button.onclick = () => ask({[key]: value});
    $(key).appendChild(button);
  }
}

function nudge(direction) {
  const current = shown("temperature");
  if (current == null) return;
  const next = Math.min(state.max_temp, Math.max(state.min_temp,
      Math.round((current + direction * state.temp_step) * 10) / 10));
  pending.temperature = {value: next, at: Date.now()};
  render();
  clearTimeout(setpointTimer);
  setpointTimer = setTimeout(() => ask({temperature: next}), 700);
}
$("up").onclick = () => nudge(1);
$("down").onclick = () => nudge(-1);

function format(temp) {
  if (temp == null) return "--";
  return (state.temp_step % 1 ? temp.toFixed(1) : Math.round(temp)) + "°" + state.temperature_unit;
}

// The panel's colors: bright while heating, cooling or drying, dark when idle.
function background(mode) {
  if (!state.connected) return "#111";
  const action = state.action;
  switch (mode) {
    case "heat": return action == "heating" ? "#e65100" : "#5c0e0e";
    case "cool": return action == "cooling" ? "#1976d2" : "#062450";
    case "dry": return action == "drying" ? "#00796b" : "#002620";
    case "heat_cool": return "#2e7d32";
    case "fan_only": return "#37474f";
  }
  return "#111";
}

function render() {
  const open = ws && ws.readyState == WebSocket.OPEN;
  const live = open && state.connected;
  const mode = shown("mode");
  document.body.style.background = background(mode);

  $("status").textContent = !open ? "Can't reach the controller, trying again…"
      : !state.connected ? "The heat pump isn't connected."
      : ACTIONS[state.action] || "";

  const usesSetpoint = live && !["off", "dry", "fan_only"].includes(mode);
  $("target").textContent = usesSetpoint ? format(shown("temperature")) : "--";
  $("target").classList.toggle("waiting", "temperature" in pending);
  $("room").textContent = live ? "Room " + format(state.roomTemperature) : "";
  $("up").disabled = $("down").disabled = !usesSetpoint;

  for (const key in CHOICES) {
    for (const button of $(key).children) {
      button.disabled = !live;
      button.classList.toggle("on", button.dataset.value == shown(key));
      button.classList.toggle("waiting", key in pending && button.dataset.value == pending[key].value);
    }
  }
}

function connect() {
  ws = new WebSocket("ws://" + location.hostname + ":81/");  // DIRECT_API_PORT
  ws.onopen = render;
  ws.onmessage = event => {
    const message = JSON.parse(event.data);
    if (message.error) {
      pending = {};
      $("error").textContent = message.error;
    } else {
      state = message;
      for (const key in pending) if (state[key] == pending[key].value) delete pending[key];
    }
    render();
  };
  ws.onclose = () => { render(); setTimeout(connect, 3000); };
}

setInterval(() => {
  let expired = false;
  for (const key in pending) {
    if (Date.now() - pending[key].at > PENDING_MS) { delete pending[key]; expired = true; }
  }
  if (expired) render();
}, 1000);

connect();
render();
</script>
)html";
