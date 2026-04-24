// ================= ELEMENTS =================
const statusEl = document.getElementById("status");
const timerEl = document.getElementById("timer");
const clockEl = document.getElementById("clock");
const connectionEl = document.getElementById("connection");
const ipEl = document.getElementById("ipAddress");
const versionEl = document.getElementById("version");
const lapBtn = document.getElementById("lapBtn");
let lapButtonPressed = false;

// ================= WEBSOCKET =================
let ws;
let reconnectTimer;

function connectWebSocket() {
  ws = new WebSocket("ws://" + location.hostname + ":81");
  
  ws.onopen = () => {
    connectionEl.textContent = "●  CONNECTED";
    connectionEl.className = "connected";
    clearTimeout(reconnectTimer);
  };
  
  ws.onclose = () => {
    connectionEl.textContent = "●  DISCONNECTED";
    connectionEl.className = "disconnected";
    reconnectTimer = setTimeout(connectWebSocket, 3000);
  };
  
  ws.onerror = () => {
    ws.close();
  };
  
  ws.onmessage = (e) => {
    try {
      // Check if message is a telegram response
      if (e.data.startsWith("telegram:")) {
        handleTelegramResponse(e.data);
        return;
      }
      
      // Otherwise parse as JSON
      const data = JSON.parse(e.data);
      updateDisplay(data);
    } catch (err) {
      console.error("Parse error:", err);
    }
  };
}

// ================= DISPLAY UPDATE =================
function updateDisplay(data) {
  console.log("📊 Data received:", data);
  
  // Update RTC time
  if (data.rtcFound && data.rtcTime) {
    rtcAvailable = true;
    const rtc = data.rtcTime;
    // Create Date object from RTC time
    const rtcDate = new Date(rtc.year, rtc.month - 1, rtc.day, rtc.hour, rtc.minute, rtc.second);
    rtcTimeOffset = rtcDate.getTime();
    lastRtcUpdate = Date.now();
  } else {
    rtcAvailable = false;
  }
  
  const remaining = data.remaining || 0;
  const elapsed = data.elapsed || 0;
  
  // Determine if we're in overtime (after countdown finished)
  const COUNTDOWN_DURATION = 300000; // 5 minutes in ms
  const isOvertime = data.running && elapsed > COUNTDOWN_DURATION;
  
  // Update status text and timer
  if (isOvertime) {
    // After countdown: toon overtime (tijd voorbij de 5 minuten)
    statusEl.textContent = "🏁 RACE GESTART";
    const overtimeMs = elapsed - COUNTDOWN_DURATION;
    const sec = Math.floor(overtimeMs / 1000);
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    timerEl.textContent = "+" + m + ":" + (s < 10 ? "0" : "") + s;
    timerEl.style.color = "#00ff88";
  } else {
    // Countdown of ready state
    if (data.phase) {
      statusEl.textContent = data.phase;
    } else if (data.sequence) {
      statusEl.textContent = "⏱️ COUNTDOWN";
    } else if (data.running) {
      statusEl.textContent = "🏁 RUNNING";
    } else {
      statusEl.textContent = "✅ READY";
    }
    
    // Update countdown timer
    const sec = Math.max(0, Math.floor(remaining / 1000));
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    timerEl.textContent = m + ":" + (s < 10 ? "0" : "") + s;
    
    // Color coding
    if (remaining > 60000) {
      timerEl.style.color = "#00ff88";
    } else if (remaining > 30000) {
      timerEl.style.color = "#ffaa00";
    } else if (remaining > 0) {
      timerEl.style.color = "#ff4444";
    } else {
      timerEl.style.color = "#00ff88";
    }
  }
  
  // Update lap times
  if (data.lapTimes && data.lapTimes.length > 0) {
    document.getElementById('lapTimesCard').style.display = 'block';
    updateLapTimes(data.lapTimes);
  } else {
    document.getElementById('lapTimesCard').style.display = 'none';
  }
  
  // Update flag display
  console.log("🚩 Updating flag to:", data.flag);
  updateFlag(data.flag);
  
  // Disable/enable buttons based on state
  const startBtn = document.getElementById('startBtn');
  const startShortBtn = document.getElementById('startShortBtn');
  const endBtn = document.getElementById('endBtn');
  const canUseLapButton = isOvertime;
  
  if (data.sequence || data.running) {
    startBtn.disabled = true;
    startShortBtn.disabled = true;
    endBtn.disabled = true;
  } else {
    startBtn.disabled = false;
    startShortBtn.disabled = false;
    endBtn.disabled = false;
  }

  lapBtn.disabled = !canUseLapButton;
  if (lapBtn.disabled && lapButtonPressed) {
    stopLapHold();
  }
  if (lapBtn.disabled) {
    lapBtn.textContent = '⏱ Tussentijd: UIT';
  }
}

// ================= LAP TIMES DISPLAY =================
function updateLapTimes(lapTimes) {
  const tbody = document.getElementById('lapTimesBody');
  tbody.innerHTML = ''; // Clear existing rows
  
  lapTimes.forEach((lapTimeMs, index) => {
    const row = document.createElement('tr');
    
    // Lap number
    const cellNum = document.createElement('td');
    cellNum.textContent = (index + 1);
    row.appendChild(cellNum);
    
    // Lap time formatted as +MM:SS
    const COUNTDOWN_DURATION = 300000; // 5 minutes
    const overtimeMs = lapTimeMs - COUNTDOWN_DURATION;
    const sec = Math.floor(overtimeMs / 1000);
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    const timeStr = "+" + m + ":" + (s < 10 ? "0" : "") + s;
    
    const cellTime = document.createElement('td');
    cellTime.textContent = timeStr;
    row.appendChild(cellTime);
    
    tbody.appendChild(row);
  });
}

// ================= FLAG DISPLAY =================
function updateFlag(flag) {
  console.log("🎌 updateFlag called with:", flag);
  const classFlag = document.getElementById('flagClass');
  const pFlag = document.getElementById('flagP');
  const labelEl = document.getElementById('flagLabel');
  
  console.log("  Elements:", { classFlag, pFlag, labelEl });
  
  // Hide all flags first
  if (classFlag) classFlag.classList.remove('active');
  if (pFlag) pFlag.classList.remove('active');
  
  // Show flags based on state
  if (flag === 'class') {
    // 5:00-4:00 en 1:00-0:00: Alleen Klassevlag
    console.log("  → Showing Klassevlag only");
    if (classFlag) classFlag.classList.add('active');
    labelEl.textContent = 'Klassevlag';
  } else if (flag === 'class-p') {
    // 4:00-1:00: Beide vlaggen
    console.log("  → Showing both flags");
    if (classFlag) classFlag.classList.add('active');
    if (pFlag) pFlag.classList.add('active');
    labelEl.textContent = 'Klassevlag + P-Vlag';
  } else {
    // Geen vlaggen
    console.log("  → Hiding all flags");
    labelEl.textContent = '';
  }
}

// ================= CONTROLS =================
function startRace() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("start");
  }
}

function cancelRace() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("cancel");
  }
}

function soundHorn() {
  console.log("[HORN] Button clicked");
  if (ws && ws.readyState === WebSocket.OPEN) {
    console.log("[HORN] Sending horn command via WebSocket");
    ws.send("horn");
  } else {
    console.error("[HORN] WebSocket not connected!", ws ? ws.readyState : "no ws");
    alert('WebSocket niet verbonden!');
  }
}

function toggleLapHold() {
  if (lapBtn.disabled) return;
  if (!(ws && ws.readyState === WebSocket.OPEN)) {
    console.error("[LAP] WebSocket not connected!");
    return;
  }

  if (lapButtonPressed) {
    // Zet UIT
    lapButtonPressed = false;
    lapBtn.classList.remove('active');
    lapBtn.textContent = '⏱ Tussentijd: UIT';
    ws.send("lapHoldStop");
  } else {
    // Zet AAN
    lapButtonPressed = true;
    lapBtn.classList.add('active');
    lapBtn.textContent = '⏱ Tussentijd: AAN';
    ws.send("lapHoldStart");
  }
}

function stopLapHold() {
  if (!lapButtonPressed) return;
  lapButtonPressed = false;
  lapBtn.classList.remove('active');
  lapBtn.textContent = '⏱ Tussentijd: UIT';
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("lapHoldStop");
  }
}

function setupLapButton() {
  // Geen extra listeners nodig - onclick="toggleLapHold()" in HTML
}

function startShortRace() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("startShort");
  }
}

function endSignal() {
  console.log("[END] Button clicked");
  if (ws && ws.readyState === WebSocket.OPEN) {
    console.log("[END] Sending end command via WebSocket");
    ws.send("end");
  } else {
    console.error("[END] WebSocket not connected!");
    alert('WebSocket niet verbonden!');
  }
}

function handleTelegramResponse(message) {
  const input = document.getElementById("telegramMessage");
  
  if (message === "telegram:ok") {
    console.log("[TELEGRAM] ✓ Message sent successfully");
    
    // Success feedback
    input.style.borderColor = '#00ff88';
    input.placeholder = 'Bericht verzonden! ✓';
    
    setTimeout(() => {
      input.style.borderColor = '#444';
      input.placeholder = 'Type hier je bericht...';
    }, 2000);
  } else if (message === "telegram:error") {
    console.error("[TELEGRAM] ✗ Failed to send message");
    
    // Error feedback
    input.style.borderColor = '#ff4444';
    input.placeholder = 'Verzenden mislukt! Controleer Telegram config.';
    
    setTimeout(() => {
      input.style.borderColor = '#444';
      input.placeholder = 'Type hier je bericht...';
    }, 3000);
    
    alert('❌ Telegram bericht kon niet worden verzonden!\n\nMogelijke oorzaken:\n- Telegram niet geconfigureerd (/telegram)\n- WiFi verbinding verloren\n- Ongeldige bot token of chat ID');
  }
}

function sendTelegramMessage() {
  const input = document.getElementById("telegramMessage");
  const message = input.value.trim();
  
  if (!message) {
    alert('Voer een bericht in!');
    return;
  }
  
  if (ws && ws.readyState === WebSocket.OPEN) {
    console.log("[TELEGRAM] Sending message:", message);
    ws.send("telegram:" + message);
    input.value = ''; // Clear input
    
    // Waiting feedback
    input.style.borderColor = '#ffaa00';
    input.placeholder = 'Bezig met verzenden...';
  } else {
    console.error("[TELEGRAM] WebSocket not connected!");
    alert('WebSocket niet verbonden!');
  }
}

// ================= CLOCK =================
let rtcTimeOffset = 0; // Offset tussen RTC en browser tijd
let lastRtcUpdate = 0;
let rtcAvailable = false;

function updateClock() {
  if (rtcAvailable && lastRtcUpdate > 0) {
    // Gebruik RTC tijd + elapsed tijd sinds laatste update
    const elapsed = Date.now() - lastRtcUpdate;
    const rtcTime = new Date(rtcTimeOffset + elapsed);
    const h = String(rtcTime.getHours()).padStart(2, '0');
    const m = String(rtcTime.getMinutes()).padStart(2, '0');
    const s = String(rtcTime.getSeconds()).padStart(2, '0');
    document.getElementById('clockTime').textContent = h + ':' + m + ':' + s;
    
    // Update RTC icon
    document.getElementById('rtcIcon').textContent = '⏱️';
    document.getElementById('rtcIcon').title = 'RTC I2C Actief';
    document.getElementById('rtcIcon').style.opacity = '1';
  } else {
    // Fallback naar browser tijd
    const now = new Date();
    const h = String(now.getHours()).padStart(2, '0');
    const m = String(now.getMinutes()).padStart(2, '0');
    const s = String(now.getSeconds()).padStart(2, '0');
    document.getElementById('clockTime').textContent = h + ':' + m + ':' + s;
    
    // Update RTC icon
    document.getElementById('rtcIcon').textContent = '🕐';
    document.getElementById('rtcIcon').title = 'Browser tijd (RTC niet beschikbaar)';
    document.getElementById('rtcIcon').style.opacity = '0.5';
  }
}

// ================= SCHEDULE =================
function loadSchedule() {
  fetch('/schedule')
    .then(r => r.json())
    .then(data => {
      const list = document.getElementById('scheduleList');
      list.innerHTML = '';
      
      if (data.length === 0) {
        list.innerHTML = '<li style="color: #666; text-align: center;">Geen tijden gepland</li>';
        return;
      }
      
      data.forEach((item, index) => {
        const li = document.createElement('li');
        li.className = 'schedule-item';
        
        const timeSpan = document.createElement('span');
        timeSpan.className = 'schedule-time' + (item.completed ? ' completed' : '');
        timeSpan.textContent = item.time;
        
        const deleteBtn = document.createElement('button');
        deleteBtn.className = 'btn-delete';
        deleteBtn.textContent = '× Verwijder';
        deleteBtn.onclick = () => deleteScheduleTime(index);
        
        li.appendChild(timeSpan);
        li.appendChild(deleteBtn);
        list.appendChild(li);
      });
    })
    .catch(err => console.error("Schedule fetch error:", err));
}

function addScheduleTime() {
  const input = document.getElementById('newTime');
  const time = input.value;
  
  console.log("[SCHEDULE] Adding time:", time);
  
  if (!time) {
    alert('Voer een tijd in');
    return;
  }
  
  fetch('/schedule', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'time=' + encodeURIComponent(time)
  })
  .then(r => {
    console.log("[SCHEDULE] Response status:", r.status);
    if (r.ok) {
      return r.json();
    } else {
      return r.text().then(text => {
        throw new Error('Server error: ' + text);
      });
    }
  })
  .then(data => {
    console.log("[SCHEDULE] Response data:", data);
    input.value = '';
    loadSchedule();
  })
  .catch(err => {
    console.error("[SCHEDULE] Add time error:", err);
    alert('Fout bij toevoegen tijd: ' + err.message);
  });
}

function deleteScheduleTime(index) {
  console.log("[SCHEDULE] Deleting time index:", index);
  
  fetch('/schedule?index=' + index, {
    method: 'DELETE'
  })
  .then(r => {
    console.log("[SCHEDULE] Delete response status:", r.status);
    if (r.ok) {
      loadSchedule();
    } else {
      alert('Fout bij verwijderen tijd');
    }
  })
  .catch(err => console.error("Delete time error:", err));
}

// ================= INIT =================
connectWebSocket();
updateClock();
setInterval(updateClock, 1000);
loadSchedule();
setInterval(loadSchedule, 10000); // Reload schedule every 10 seconds
setupLapButton();
// ================= VERSION =================
fetch('/status')
  .then(r => r.json())
  .then(data => {
    if (data.ip) {
      ipEl.textContent = "IP: " + data.ip;
    }
    if (data.version) {
      versionEl.textContent = data.version;
    }
  })
  .catch(err => console.error("Status fetch error:", err));

// ================= INIT =================
connectWebSocket();
updateClock();
setInterval(updateClock, 1000);
