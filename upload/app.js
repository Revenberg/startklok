let running = false;

// ================= START =================
function startRace() {
  fetch('/start');
}

// ================= STOP =================
function stopRace() {
  fetch('/cancel');
}

// ================= STATUS POLL =================
setInterval(() => {

  fetch('/status')
    .then(res => res.json())
    .then(data => {

      running = data.running;

      document.getElementById("status").innerText =
        running ? "RUNNING" : "IDLE";
    });

}, 1000);

// ================= RELAY CONTROL =================
function relay(id, state) {

  fetch(`/relay?nr=${id}&state=${state}`);
}

function updateClock() {
  const now = new Date();

  let h = now.getHours();
  let m = now.getMinutes();
  let s = now.getSeconds();

  if (m < 10) m = "0" + m;
  if (s < 10) s = "0" + s;

  document.getElementById("clock").innerText =
    h + ":" + m + ":" + s;
}

setInterval(updateClock, 1000);
updateClock();
