let isSaving = false;
let isAPMode = false;

// Set initial value display for brightness
document.addEventListener('DOMContentLoaded', function() {
    brightnessValue.textContent = brightnessSlider.value;
});

function toggleWeatherApiKey(provider) {
  const apiKeySection = document.getElementById('weatherApiKeySection');
  apiKeySection.style.display = (provider === 'openweather' || provider === 'pirateweather') ? 'block' : 'none';
}

// Show/Hide Password toggle
document.addEventListener("DOMContentLoaded", function () {
    const passwordInput = document.getElementById("password");
    const toggleCheckbox = document.getElementById("togglePassword");

    toggleCheckbox.addEventListener("change", function () {
        if (this.checked) {
            // Show password as text
            passwordInput.type = "text";

            // Only clear if it's the masked placeholder
            if (passwordInput.value === "********") {
                passwordInput.value = "";
                passwordInput.placeholder = "Enter new password";
            }
        } else {
            // Hide password as dots
            passwordInput.type = "password";

            // Remove placeholder only if it was set by show-password toggle
            if (passwordInput.placeholder === "Enter new password") {
                passwordInput.placeholder = "";
            }
        }
    });
});

// Show/Hide Weather API Key toggle
document.addEventListener("DOMContentLoaded", function () {
  const weatherApiKeyInput = document.getElementById("weatherApiKey");
  const toggleWeatherApiKeyCheckbox = document.getElementById("toggleWeatherApiKey");

  if (toggleWeatherApiKeyCheckbox && weatherApiKeyInput) {
    toggleWeatherApiKeyCheckbox.addEventListener("change", function () {
      if (this.checked) {
        weatherApiKeyInput.type = "text";
        if (weatherApiKeyInput.value === "********") {
          weatherApiKeyInput.value = "";
          weatherApiKeyInput.placeholder = "Enter new API key";
        }
      } else {
        weatherApiKeyInput.type = "password";
        if (weatherApiKeyInput.placeholder === "Enter new API key") {
          weatherApiKeyInput.placeholder = "Enter API key";
        }
      }
    });
  }
});

// Auth settings toggle
document.getElementById('authEnabled').addEventListener('change', function() {
    document.getElementById('authSettings').style.display = this.checked ? 'block' : 'none';
});

document.getElementById('totpEnabled').addEventListener('change', function() {
    document.getElementById('totpSetup').style.display = this.checked ? 'block' : 'none';
});

document.getElementById('toggleAdminPassword').addEventListener('change', function() {
    const passwordInput = document.getElementById('adminPassword');
    passwordInput.type = this.checked ? 'text' : 'password';

    if (this.checked && passwordInput.value === '********') {
        passwordInput.value = '';
        passwordInput.placeholder = 'Enter new password';
    }
});

async function setupTOTP() {
    try {
        await fetch('/auth/generate', {
            method: 'POST',
            headers: {'X-Auth-Token': sessionStorage.getItem('token') || ''}
        });

        const response = await fetch('/auth/qrcode');
        const data = await response.json();

        document.getElementById('qrcode').innerHTML = '';

        new QRCode(document.getElementById('qrcode'), {
            text: data.uri,
            width: 200,
            height: 200,
            colorDark: '#ffffff',
            colorLight: 'transparent',
            correctLevel: QRCode.CorrectLevel.M
        });

        document.getElementById('totpSecret').style.display = 'block';
        document.querySelector('#totpSecret code').textContent = data.secret;

    } catch (error) {
        alert('Error generating TOTP secret');
    }
}

// Show/Hide Webhook Key toggle
document.addEventListener("DOMContentLoaded", function () {
    const webhookKeyInput = document.getElementById("webhookKey");
    const toggleWebhookCheckbox = document.getElementById("toggleWebhookKey");

    if (toggleWebhookCheckbox && webhookKeyInput) {
        toggleWebhookCheckbox.addEventListener("change", function () {
            if (this.checked) {
                webhookKeyInput.type = "text";
                if (webhookKeyInput.value === "********") {
                    webhookKeyInput.value = "";
                    webhookKeyInput.placeholder = "Enter new secret key";
                }
            } else {
                webhookKeyInput.type = "password";
                if (webhookKeyInput.placeholder === "Enter new secret key") {
                    webhookKeyInput.placeholder = "Enter secret key";
                }
            }
        });
    }
});

// Show/Hide YouTube API Key toggle
document.addEventListener("DOMContentLoaded", function () {
    const youtubeApiKeyInput = document.getElementById("youtubeApiKey");
    const toggleYoutubeCheckbox = document.getElementById("toggleYoutubeApiKey");

    if (toggleYoutubeCheckbox && youtubeApiKeyInput) {
        toggleYoutubeCheckbox.addEventListener("change", function () {
            if (this.checked) {
                youtubeApiKeyInput.type = "text";
                if (youtubeApiKeyInput.value === "********") {
                    youtubeApiKeyInput.value = "";
                    youtubeApiKeyInput.placeholder = "Enter new API key";
                }
            } else {
                youtubeApiKeyInput.type = "password";
                if (youtubeApiKeyInput.placeholder === "Enter new API key") {
                    youtubeApiKeyInput.placeholder = "AIza...";
                }
            }
        });
    }
});

window.onbeforeunload = function () {
    if (isSaving) {
        return "Settings are being saved. Leaving now may interrupt the process.";
    }
};

window.onload = function () {
    fetch('/config.json', {
        headers: {
            'X-Auth-Token': sessionStorage.getItem('token') || ''
        }
    })
        .then(response => {
            if (response.status === 401) {
                return fetch('/config.json').then(r => {
                    if (r.ok) {
                        return r.json();
                    } else {
                        window.location.href = '/login';
                        return;
                    }
                });
            }
            if (!response.ok) {
                throw new Error('Failed to load config');
            }
            return response.json();
        })
        .then(data => {
            if (!data) return;

            isAPMode = (data.mode === "ap");
            if (isAPMode) {
                document.querySelector('.geo-note').style.display = 'block';
                document.getElementById('geo-button').classList.add('geo-disabled');
                document.getElementById('geo-button').disabled = true;
            }

            document.getElementById('ssid').value = data.ssid || '';
            document.getElementById('password').value = data.password || '';
            document.getElementById('openMeteoLatitude').value = data.openMeteoLatitude || '';
            document.getElementById('openMeteoLongitude').value = data.openMeteoLongitude || '';
            document.getElementById('weatherUnits').checked = (data.weatherUnits === "imperial");
            document.getElementById('clockDuration').value = (data.clockDuration || 10000) / 1000;
            document.getElementById('weatherDuration').value = (data.weatherDuration || 5000) / 1000;
            document.getElementById('language').value = data.language || '';
            // Weather provider
            document.getElementById('weatherProvider').value = data.weatherProvider || 'openmeteo';
            document.getElementById('weatherApiKey').value = data.weatherApiKey || '';
            toggleWeatherApiKey(data.weatherProvider || 'openmeteo');
            // Advanced:
            document.getElementById('brightnessSlider').value = typeof data.brightness !== "undefined" ? data.brightness : 10;
            document.getElementById('brightnessValue').textContent = (document.getElementById('brightnessSlider').value == -1 ? 'Off' : document.getElementById('brightnessSlider').value);
            document.getElementById('flipDisplay').checked = !!data.flipDisplay;
            document.getElementById('ntpServer1').value = data.ntpServer1 || "";
            document.getElementById('ntpServer2').value = data.ntpServer2 || "";
            document.getElementById('twelveHourToggle').checked = !!data.twelveHourToggle;
            document.getElementById('showDayOfWeek').checked = !!data.showDayOfWeek;
            document.getElementById('showDate').checked = !!data.showDate;
            document.getElementById('showHumidity').checked = !!data.showHumidity;
            document.getElementById('colonBlinkEnabled').checked = !!data.colonBlinkEnabled;
            document.getElementById('showWeatherDescription').checked = !!data.showWeatherDescription;
            document.getElementById('apiEnabled').checked = !!data.apiEnabled;
            // Webhooks
            document.getElementById('webhooksEnabled').checked = !!data.webhooksEnabled;
            document.getElementById('webhookKey').value = data.webhookKey || '';
            document.getElementById('webhookQueueSize').value = data.webhookQueueSize || 5;
            document.getElementById('queueSizeValue').textContent = data.webhookQueueSize || 5;
            document.getElementById('webhookQuietHours').checked = data.webhookQuietHours !== false;

            // Show/hide webhook settings
            if (data.webhooksEnabled) {
                document.getElementById('webhookSettings').style.display = 'block';
            }

            // Update webhook URL with actual IP
            if (!isAPMode && window.location.hostname) {
                document.getElementById('webhookUrl').textContent =
                    `http://${window.location.hostname}/webhook`;
            }
            // YT
            document.getElementById('youtubeEnabled').checked = !!(data.youtube && data.youtube.enabled);
            document.getElementById('youtubeApiKey').value = (data.youtube && data.youtube.apiKey) || '';
            document.getElementById('youtubeChannelId').value = (data.youtube && data.youtube.channelId) || '';
            document.getElementById('youtubeShortFormat').checked = data.youtube ? data.youtube.shortFormat : true;
            // Auth
            document.getElementById('authEnabled').checked = !!data.authEnabled;
            document.getElementById('adminPassword').value = data.adminPassword || '';
            document.getElementById('totpEnabled').checked = !!data.totpEnabled;
            if (data.authEnabled) {
                document.getElementById('authSettings').style.display = 'block';
            }
            if (data.totpEnabled) {
                document.getElementById('totpSetup').style.display = 'block';
            }
            // Dimming controls
            const dimmingEnabledEl = document.getElementById('dimmingEnabled');
            const isDimming = (data.dimmingEnabled === true || data.dimmingEnabled === "true" || data.dimmingEnabled === 1);
            dimmingEnabledEl.checked = isDimming;

// Defer field enabling until checkbox state is rendered
            setTimeout(() => {
                setDimmingFieldsEnabled(dimmingEnabledEl.checked);
                setYoutubeFieldsEnabled(document.getElementById('youtubeEnabled').checked);
            }, 0);

            dimmingEnabledEl.addEventListener('change', function () {
                setDimmingFieldsEnabled(this.checked);
            });

            document.getElementById('youtubeEnabled').addEventListener('change', function() {
                setYoutubeEnabled(this.checked);
                setYoutubeFieldsEnabled(this.checked);
            });

            document.getElementById('dimStartTime').value =
                (data.dimStartHour !== undefined ? String(data.dimStartHour).padStart(2, '0') : "18") + ":" +
                (data.dimStartMinute !== undefined ? String(data.dimStartMinute).padStart(2, '0') : "00");

            document.getElementById('dimEndTime').value =
                (data.dimEndHour !== undefined ? String(data.dimEndHour).padStart(2, '0') : "08") + ":" +
                (data.dimEndMinute !== undefined ? String(data.dimEndMinute).padStart(2, '0') : "00");

            document.getElementById('dimBrightness').value = (data.dimBrightness !== undefined ? data.dimBrightness : 2);
            // Then update the span's text content with that value
            document.getElementById('dimmingBrightnessValue').textContent = (document.getElementById('dimBrightness').value == -1 ? 'Off' : document.getElementById('dimBrightness').value);

            setDimmingFieldsEnabled(!!data.dimmingEnabled);

            // --- NEW: Populate Countdown Fields ---
            document.getElementById('isDramaticCountdown').checked = !!(data.countdown && data.countdown.isDramaticCountdown);
            const countdownEnabledEl = document.getElementById('countdownEnabled'); // Get reference
            countdownEnabledEl.checked = !!(data.countdown && data.countdown.enabled);

            if (data.countdown && data.countdown.targetTimestamp) {
                // Convert Unix timestamp (seconds) to milliseconds for JavaScript Date object
                const targetDate = new Date(data.countdown.targetTimestamp * 1000);
                const year = targetDate.getFullYear();
                // Month is 0-indexed in JS, so add 1
                const month = (targetDate.getMonth() + 1).toString().padStart(2, '0');
                const day = targetDate.getDate().toString().padStart(2, '0');
                const hours = targetDate.getHours().toString().padStart(2, '0');
                const minutes = targetDate.getMinutes().toString().padStart(2, '0');

                document.getElementById('countdownDate').value = `${year}-${month}-${day}`;
                document.getElementById('countdownTime').value = `${hours}:${minutes}`;
            } else {
                // Clear fields if no target timestamp is set
                document.getElementById('countdownDate').value = '';
                document.getElementById('countdownTime').value = '';
            }
            // --- END NEW ---

            // --- NEW: Countdown Label Input Validation ---
            const countdownLabelInput = document.getElementById('countdownLabel');
            countdownLabelInput.addEventListener('input', function() {
                // Convert to uppercase and remove any characters that are not A-Z or space
                // Note: The `maxlength` attribute in HTML handles the length limit.
                this.value = this.value.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
            });
            // Set initial value for countdownLabel from config.json (apply validation on load too)
            if (data.countdown && data.countdown.label) {
                countdownLabelInput.value = data.countdown.label.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
            } else {
                countdownLabelInput.value = '';
            }
            // --- END NEW ---


            // --- NEW: Countdown Toggle Event Listener & Field Enabling ---
            // If you're using onchange="setCountdownEnabled(this.checked)" directly in HTML,
            // you would add setCountdownFieldsEnabled(this.checked) there as well.
            // If you are using addEventListener, keep this:
            countdownEnabledEl.addEventListener('change', function() {
                setCountdownEnabled(this.checked);       // Sends command to ESP
                setCountdownFieldsEnabled(this.checked); // Enables/disables local fields
            });

            const dramaticCountdownEl = document.getElementById('isDramaticCountdown');
            dramaticCountdownEl.addEventListener('change', function () {
                setIsDramaticCountdown(this.checked);
            });

            // Set initial state of fields when page loads
            setCountdownFieldsEnabled(countdownEnabledEl.checked);
            // --- END NEW ---

            // Auto-detect browser's timezone if not set in config
            if (!data.timeZone) {
                try {
                    const tz = Intl.DateTimeFormat().resolvedOptions().timeZone;
                    if (
                        tz &&
                        document.getElementById('timeZone').querySelector(`[value="${tz}"]`)
                    ) {
                        document.getElementById('timeZone').value = tz;
                    } else {
                        document.getElementById('timeZone').value = '';
                    }
                } catch (e) {
                    document.getElementById('timeZone').value = '';
                }
            } else {
                document.getElementById('timeZone').value = data.timeZone;
            }
        })
        .catch(err => {
            console.error('Failed to load config:', err);
            showSavingModal("");
            updateSavingModal("⚠️ Failed to load configuration.", false);

            // Show appropriate button for load error
            removeReloadButton();
            removeRestoreButton();
            const errorMsg = (err.message || "").toLowerCase();
            if (
                errorMsg.includes("config corrupted") ||
                errorMsg.includes("failed to write config") ||
                errorMsg.includes("restore")
            ) {
                ensureRestoreButton();
            } else {
                ensureReloadButton();
            }
        });
    document.querySelector('html').style.height = 'unset';
    document.body.classList.add('loaded');
};

async function submitConfig(event) {
    event.preventDefault();
    isSaving = true;

    const form = document.getElementById('configForm');
    const formData = new FormData(form);

    const clockDuration = parseInt(formData.get('clockDuration')) * 1000;
    const weatherDuration = parseInt(formData.get('weatherDuration')) * 1000;
    formData.set('clockDuration', clockDuration);
    formData.set('weatherDuration', weatherDuration);

    // Advanced: ensure correct values are set for advanced fields
    formData.set('brightness', document.getElementById('brightnessSlider').value);
    formData.set('flipDisplay', document.getElementById('flipDisplay').checked ? 'on' : '');
    formData.set('twelveHourToggle', document.getElementById('twelveHourToggle').checked ? 'on' : '');
    formData.set('showDayOfWeek', document.getElementById('showDayOfWeek').checked ? 'on' : '');
    formData.set('showDate', document.getElementById('showDate').checked ? 'on' : '');
    formData.set('showHumidity', document.getElementById('showHumidity').checked ? 'on' : '');
    formData.set('colonBlinkEnabled', document.getElementById('colonBlinkEnabled').checked ? 'on' : '');

    //dimming
    formData.set('dimmingEnabled', document.getElementById('dimmingEnabled').checked ? 'true' : 'false');
    const dimStart = document.getElementById('dimStartTime').value; // "18:45"
    const dimEnd = document.getElementById('dimEndTime').value;     // "08:30"

    // Parse hour and minute
    if (dimStart) {
        const [startHour, startMin] = dimStart.split(":").map(x => parseInt(x, 10));
        formData.set('dimStartHour', startHour);
        formData.set('dimStartMinute', startMin);
    }
    if (dimEnd) {
        const [endHour, endMin] = dimEnd.split(":").map(x => parseInt(x, 10));
        formData.set('dimEndHour', endHour);
        formData.set('dimEndMinute', endMin);
    }
    formData.set('dimBrightness', document.getElementById('dimBrightness').value);
    formData.set('showWeatherDescription', document.getElementById('showWeatherDescription').checked ? 'on' : '');
    formData.set('weatherUnits', document.getElementById('weatherUnits').checked ? 'imperial' : 'metric');
    formData.set('apiEnabled', document.getElementById('apiEnabled').checked ? 'on' : '');

    // --- NEW: Countdown Form Data ---
    formData.set('countdownEnabled', document.getElementById('countdownEnabled').checked ? 'true' : 'false');
    formData.set('isDramaticCountdown', document.getElementById('isDramaticCountdown').checked ? 'true' : 'false');
    // Date and Time inputs are already handled by formData if they have a 'name' attribute
    // 'countdownDate' and 'countdownTime' are collected automatically
    // Also apply the same validation for the label when submitting
    const finalCountdownLabel = document.getElementById('countdownLabel').value.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
    formData.set('countdownLabel', finalCountdownLabel);
    // --- NEW: Webhook Form Data ---
    formData.set('webhooksEnabled', document.getElementById('webhooksEnabled').checked ? 'true' : 'false');
    formData.set('webhookKey', document.getElementById('webhookKey').value);
    formData.set('webhookQueueSize', document.getElementById('webhookQueueSize').value);
    formData.set('webhookQuietHours', document.getElementById('webhookQuietHours').checked ? 'true' : 'false');
    // --- NEW: YouTube Form Data ---
    formData.set('youtubeEnabled', document.getElementById('youtubeEnabled').checked ? 'true' : 'false');
    formData.set('youtubeApiKey', document.getElementById('youtubeApiKey').value);
    formData.set('youtubeChannelId', document.getElementById('youtubeChannelId').value);
    formData.set('youtubeShortFormat', document.getElementById('youtubeShortFormat').checked ? 'true' : 'false');
    // --- NEW: Auth Form Data ---
    formData.set('authEnabled', document.getElementById('authEnabled').checked ? 'on' : '');
    formData.set('adminPassword', document.getElementById('adminPassword').value);
    formData.set('totpEnabled', document.getElementById('totpEnabled').checked ? 'on' : '');
    // --- END NEW ---

    const params = new URLSearchParams();
    for (const pair of formData.entries()) {
        params.append(pair[0], pair[1]);
    }

    showSavingModal("Saving...");

    // Check AP mode status
    let isAPMode = false;
    try {
        const apStatusResponse = await fetch('/ap_status');
        const apStatusData = await apStatusResponse.json();
        isAPMode = apStatusData.isAP;
    } catch (error) {
        console.error("Error fetching AP status:", error);
        // Handle error appropriately (e.g., assume not in AP mode)
    }

    fetch('/save', {
        method: 'POST',
        headers: {
            'X-Auth-Token': sessionStorage.getItem('token') || ''
        },
        body: params
    })
        .then(response => {
            if (!response.ok) {
                return response.json().then(json => {
                    throw new Error(`Server error ${response.status}: ${json.error}`);
                });
            }
            return response.json();
        })
        .then(json => {
            isSaving = false;
            removeReloadButton();
            removeRestoreButton();
            if (isAPMode) {
                updateSavingModal("✅ Settings saved successfully!<br><br>Rebooting the device now... ", false);
                setTimeout(() => {
                    document.getElementById('configForm').style.display = 'none';
                    document.querySelector('.footer').style.display = 'none';
                    document.querySelector('html').style.height = '100vh';
                    document.body.style.height = '100vh';
                    document.getElementById('configForm').style.display = 'none';
                    updateSavingModal("✅ All done!<br>You can now close this tab safely.<br><br>Your device is now rebooting and connecting to your Wi-Fi. Its new IP address will appear on the display for future access.", false);
                }, 5000);
            } else {
                updateSavingModal("✅ Configuration saved successfully.<br><br>Device will reboot", false);
                setTimeout(() => location.href = location.href.split('#')[0], 3000);
            }
        })
        .catch(err => {
            isSaving = false;

            if (isAPMode && err.message.includes("Failed to fetch")) {
                console.warn("Expected disconnect in AP mode after reboot.");
                showSavingModal("");
                updateSavingModal("✅ Settings saved successfully!<br><br>Rebooting the device now... ", false);
                setTimeout(() => {
                    document.getElementById('configForm').style.display = 'none';
                    updateSavingModal("✅ All done!<br>You can now close this tab safely.<br><br>Your device is now rebooting and connecting to your Wi-Fi. Its new IP address will appear on the display for future access.", false);
                }, 5000);
                removeReloadButton();
                removeRestoreButton();
                return;
            }

            console.error('Save error:', err);
            let friendlyMessage = "⚠️ Something went wrong while saving the configuration.";
            if (err.message.includes("Failed to fetch")) {
                friendlyMessage = "⚠️ Cannot connect to the device.<br>Is it powered on and connected?";
            }

            updateSavingModal(`${friendlyMessage}<br><br>Details: ${err.message}`, false);

            // Show only one action button, based on error content
            removeReloadButton();
            removeRestoreButton();
            const errorMsg = (err.message || "").toLowerCase();
            if (
                errorMsg.includes("config corrupted") ||
                errorMsg.includes("failed to write config") ||
                errorMsg.includes("restore")
            ) {
                ensureRestoreButton();
            } else {
                ensureReloadButton();
            }
        });
}

function showSavingModal(message) {
    let modal = document.getElementById('savingModal');
    if (!modal) {
        modal = document.createElement('div');
        modal.id = 'savingModal';
        modal.innerHTML = `
      <div id="savingModalContent">
        <div class="spinner"></div>
        <div id="savingModalText">${message}</div>
      </div>
    `;
        document.body.appendChild(modal);
    } else {
        document.getElementById('savingModalText').innerHTML = message;
        document.querySelector('#savingModal .spinner').style.display = 'block';
    }
    modal.style.display = 'flex';
    document.body.classList.add('modal-open');
}

function updateSavingModal(message, showSpinner = false) {
    let modalText = document.getElementById('savingModalText');
    modalText.innerHTML = message;
    document.querySelector('#savingModal .spinner').style.display = showSpinner ? 'block' : 'none';

    // Remove reload/restore buttons if no longer needed
    if (message.includes("saved successfully") || message.includes("Backup restored") || message.includes("All done!")) {
        removeReloadButton();
        removeRestoreButton();
    }
}

function ensureReloadButton(options = {}) {
    let modalContent = document.getElementById('savingModalContent');
    if (!modalContent) return;
    let btn = document.getElementById('reloadButton');
    if (!btn) {
        btn = document.createElement('button');
        btn.id = 'reloadButton';
        btn.className = 'primary-button';
        btn.style.display = 'inline-block';
        btn.style.margin = '1rem 0.5rem 0 0';
        modalContent.appendChild(btn);
    }
    btn.textContent = options.text || "Reload Page";
    btn.onclick = options.onClick || (() => location.reload());
    btn.style.display = 'inline-block';
    return btn;
}

function ensureRestoreButton(options = {}) {
    let modalContent = document.getElementById('savingModalContent');
    if (!modalContent) return;
    let btn = document.getElementById('restoreButton');
    if (!btn) {
        btn = document.createElement('button');
        btn.id = 'restoreButton';
        btn.className = 'primary-button';
        btn.style.display = 'inline-block';
        btn.style.margin = '1rem 0 0 0.5rem';
        modalContent.appendChild(btn);
    }
    btn.textContent = options.text || "Restore Backup";
    btn.onclick = options.onClick || restoreBackupConfig;
    btn.style.display = 'inline-block';
    return btn;
}

function removeReloadButton() {
    let btn = document.getElementById('reloadButton');
    if (btn && btn.parentNode) btn.parentNode.removeChild(btn);
}
function removeRestoreButton() {
    let btn = document.getElementById('restoreButton');
    if (btn && btn.parentNode) btn.parentNode.removeChild(btn);
}

function restoreBackupConfig() {
    showSavingModal("Restoring backup...");
    removeReloadButton();
    removeRestoreButton();

    fetch('/restore', {
        method: 'POST',
        headers: {
            'X-Auth-Token': sessionStorage.getItem('token') || ''
        }
    })
        .then(response => {
            if (!response.ok) {
                throw new Error("Server returned an error");
            }
            return response.json();
        })
        .then(data => {
            updateSavingModal("✅ Backup restored! Device will now reboot.");
            setTimeout(() => location.reload(), 1500);
        })
        .catch(err => {
            console.error("Restore error:", err);
            updateSavingModal(`❌ Failed to restore backup: ${err.message}`, false);

            // Show only one button, for backup restore failures show reload.
            removeReloadButton();
            removeRestoreButton();
            ensureReloadButton();
        });
}

function hideSavingModal() {
    const modal = document.getElementById('savingModal');
    if (modal) {
        modal.style.display = 'none';
        document.body.classList.remove('modal-open');
    }
}

const toggle = document.querySelector('.collapsible-toggle');
const content = document.querySelector('.collapsible-content');
toggle.addEventListener('click', function() {
    const isOpen = toggle.classList.toggle('open');
    toggle.setAttribute('aria-expanded', isOpen);
    content.setAttribute('aria-hidden', !isOpen);
    if(isOpen) {
        content.style.height = content.scrollHeight + 'px';
        content.addEventListener('transitionend', function handler() {
            content.style.height = 'auto';
            content.removeEventListener('transitionend', handler);
        });
    } else {
        content.style.height = content.scrollHeight + 'px';
        // Force reflow to make sure the browser notices the height before transitioning to 0
        void content.offsetHeight;
        content.style.height = '0px';
    }
});
// Optional: If open on load, set height to auto
if(toggle.classList.contains('open')) {
    content.style.height = 'auto';
}

let brightnessDebounceTimeout = null;

function setBrightnessLive(val) {
    // Cancel the previous timeout if it exists
    if (brightnessDebounceTimeout) {
        clearTimeout(brightnessDebounceTimeout);
    }
    // Set a new timeout
    brightnessDebounceTimeout = setTimeout(() => {
        fetch('/set_brightness', {
            method: 'POST',
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "value=" + encodeURIComponent(val)
        })
            .then(res => res.json())
            .catch(e => {}); // Optionally handle errors
    }, 150); // 150ms debounce, adjust as needed
}

function setFlipDisplay(val) {
    fetch('/set_flip', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setTwelveHour(val) {
    fetch('/set_twelvehour', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setShowDayOfWeek(val) {
    fetch('/set_dayofweek', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setShowDate(val) {
    fetch('/set_showdate', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setColonBlink(val) {
    fetch('/set_colon_blink', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setShowHumidity(val) {
    fetch('/set_humidity', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setLanguage(val) {
    fetch('/set_language', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + encodeURIComponent(val)
    });
}

function setShowWeatherDescription(val) {
    fetch('/set_weatherdesc', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setWeatherUnits(val) {
    fetch('/set_units', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setApiEnabled(val) {
    fetch('/set_api', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

// --- Countdown Controls Logic ---
// NEW: Function to enable/disable countdown specific fields
function setCountdownFieldsEnabled(enabled) {
    document.getElementById('countdownLabel').disabled = !enabled;
    document.getElementById('countdownDate').disabled = !enabled;
    document.getElementById('countdownTime').disabled = !enabled;
    document.getElementById('isDramaticCountdown').disabled = !enabled;
}

// Existing function to send countdown enable/disable command to ESP
function setCountdownEnabled(val) {
    fetch('/set_countdown_enabled', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0) // Send 1 for true, 0 for false
    });
}

function setIsDramaticCountdown(val) {
    fetch('/set_dramatic_countdown', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0) // Send 1 for true, 0 for false
    });
}

// --- END Countdown Controls Logic ---


// --- Dimming Controls Logic ---
function setDimmingFieldsEnabled(enabled) {
    document.getElementById('dimStartTime').disabled = !enabled;
    document.getElementById('dimEndTime').disabled = !enabled;
    document.getElementById('dimBrightness').disabled = !enabled;
}

function getLocation() {
    fetch('http://ip-api.com/json/')
        .then(response => response.json())
        .then(data => {
            // Update your latitude/longitude fields
            document.getElementById('openMeteoLatitude').value = data.lat;
            document.getElementById('openMeteoLongitude').value = data.lon;

            // Determine the label to show on the button
            const button = document.getElementById('geo-button');
            let label = data.city;
            if (!label) label = data.regionName;
            if (!label) label = data.country;
            if (!label) label = "Location Found";

            button.textContent = "Location: " + label;
            button.disabled = true;
            button.classList.add('geo-disabled');

            console.log("Location fetched via ip-api.com. Free service: http://ip-api.com/");
        })
        .catch(error => {
            alert(
                "Failed to guess your location.\n\n" +
                "This may happen if:\n" +
                "- You are using an AdBlocker\n" +
                "- There is a network issue\n" +
                "- The service might be temporarily down.\n\n" +
                "You can visit https://www.openstreetmap.org to manually search for your city and get latitude/longitude."
            );
        });
}

// Webhook
function setWebhooks(val) {
    fetch('/set_webhooks', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });

    document.getElementById('webhookSettings').style.display = val ? 'block' : 'none';
}

function testWebhook() {
    const key = document.getElementById('webhookKey').value;
    if (!key) {
        alert('⚠️ Please set a webhook key first!');
        return;
    }

    // Show loading
    const btn = event.target;
    const originalText = btn.textContent;
    btn.disabled = true;
    btn.textContent = '⏳ Testing...';

    fetch('/webhook', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: `key=${encodeURIComponent(key)}&message=TEST OK&duration=3&priority=2`
    })
        .then(r => r.json())
        .then(data => {
            if (data.status === 'queued') {
                alert('✅ Webhook test successful!\nLook at your display!');
            } else {
                alert('❌ Error: ' + (data.error || 'Unknown error'));
            }
        })
        .catch(e => {
            alert('❌ Connection failed: ' + e.message);
        })
        .finally(() => {
            btn.disabled = false;
            btn.textContent = originalText;
        });
}

// --- YouTube Controls Logic ---
function setYoutubeFieldsEnabled(enabled) {
    document.getElementById('youtubeApiKey').disabled = !enabled;
    document.getElementById('youtubeChannelId').disabled = !enabled;
    document.getElementById('youtubeShortFormat').disabled = !enabled;

    // Show/hide the settings div
    document.getElementById('youtubeSettings').style.display = enabled ? 'block' : 'none';
}

function setYoutubeEnabled(val) {
    fetch('/set_youtube_enabled', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}

function setYoutubeShortFormat(val) {
    fetch('/set_youtube_format', {
        method: 'POST',
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "value=" + (val ? 1 : 0)
    });
}