let isSaving = false;
let isAPMode = false;

// Helpers
function setupPasswordToggle(inputId, toggleId, showPlaceholder, hidePlaceholder = '') {
    const input = document.getElementById(inputId);
    const toggle = document.getElementById(toggleId);
    if (!input || !toggle) return;

    toggle.addEventListener('change', function () {
        if (this.checked) {
            input.type = 'text';
            if (input.value === '********') {
                input.value = '';
                input.placeholder = showPlaceholder;
            }
        } else {
            input.type = 'password';
            if (input.placeholder === showPlaceholder && hidePlaceholder) {
                input.placeholder = hidePlaceholder;
            }
        }
    });
}

function postBoolValue(endpoint, val) {
    fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'value=' + (val ? 1 : 0)
    });
}

function postEncodedValue(endpoint, val) {
    fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'value=' + encodeURIComponent(val)
    });
}

function loadCheckboxes(data, fields) {
    fields.forEach(field => {
        const el = document.getElementById(field);
        if (el) el.checked = !!data[field];
    });
}

function appendCheckboxesToFormData(formData, fields) {
    fields.forEach(field => {
        const el = document.getElementById(field);
        if (el) formData.set(field, el.checked ? 'on' : '');
    });
}

function appendBoolCheckboxesToFormData(formData, fields) {
    fields.forEach(field => {
        const el = document.getElementById(field);
        if (el) formData.set(field, el.checked ? 'true' : 'false');
    });
}

function toggleWeatherApiKey(provider) {
    const apiKeySection = document.getElementById('weatherApiKeySection');
    apiKeySection.style.display = (provider === 'openweather' || provider === 'pirateweather') ? 'block' : 'none';
}

document.addEventListener('DOMContentLoaded', function () {
    // Set initial value display for brightness
    const brightnessSlider = document.getElementById('brightnessSlider');
    const brightnessValue = document.getElementById('brightnessValue');
    if (brightnessSlider && brightnessValue) {
        brightnessValue.textContent = brightnessSlider.value;
    }

    // Show/Hide Password toggles
    setupPasswordToggle('password', 'togglePassword', 'Enter new password', '');
    setupPasswordToggle('weatherApiKey', 'toggleWeatherApiKey', 'Enter new API key', 'Enter API key');
    setupPasswordToggle('adminPassword', 'toggleAdminPassword', 'Enter new password', '');
    setupPasswordToggle('webhookKey', 'toggleWebhookKey', 'Enter new secret key', 'Enter secret key');
    setupPasswordToggle('youtubeApiKey', 'toggleYoutubeApiKey', 'Enter new API key', 'AIza...');

    // Auth settings toggle
    const authEnabled = document.getElementById('authEnabled');
    if (authEnabled) {
        authEnabled.addEventListener('change', function () {
            document.getElementById('authSettings').style.display = this.checked ? 'block' : 'none';
        });
    }

    const totpEnabled = document.getElementById('totpEnabled');
    if (totpEnabled) {
        totpEnabled.addEventListener('change', function () {
            document.getElementById('totpSetup').style.display = this.checked ? 'block' : 'none';
        });
    }
});

// TOTP
async function setupTOTP() {
    try {
        await fetch('/auth/generate', {
            method: 'POST',
            headers: { 'X-Auth-Token': sessionStorage.getItem('token') || '' }
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

window.onbeforeunload = function () {
    if (isSaving) {
        return 'Settings are being saved. Leaving now may interrupt the process.';
    }
};

// Config loading
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

            isAPMode = (data.mode === 'ap');
            if (isAPMode) {
                document.querySelector('.geo-note').style.display = 'block';
                document.getElementById('geo-button').classList.add('geo-disabled');
                document.getElementById('geo-button').disabled = true;
            }

            document.getElementById('ssid').value = data.ssid || '';
            document.getElementById('password').value = data.password || '';
            document.getElementById('openMeteoLatitude').value = data.openMeteoLatitude || '';
            document.getElementById('openMeteoLongitude').value = data.openMeteoLongitude || '';
            document.getElementById('language').value = data.language || '';
            document.getElementById('ntpServer1').value = data.ntpServer1 || '';
            document.getElementById('ntpServer2').value = data.ntpServer2 || '';
            document.getElementById('mdnsHostname').value = data.mdnsHostname || 'esptimecast';
            document.getElementById('webhookKey').value = data.webhookKey || '';
            document.getElementById('adminPassword').value = data.adminPassword || '';

            document.getElementById('weatherUnits').checked = (data.weatherUnits === 'imperial');
            document.getElementById('clockDuration').value = (data.clockDuration || 10000) / 1000;
            document.getElementById('weatherDuration').value = (data.weatherDuration || 5000) / 1000;
            document.getElementById('weatherProvider').value = data.weatherProvider || 'openmeteo';
            document.getElementById('weatherApiKey').value = data.weatherApiKey || '';
            toggleWeatherApiKey(data.weatherProvider || 'openmeteo');

            document.getElementById('brightnessSlider').value = typeof data.brightness !== 'undefined' ? data.brightness : 10;
            document.getElementById('brightnessValue').textContent = (document.getElementById('brightnessSlider').value == -1 ? 'Off' : document.getElementById('brightnessSlider').value);

            loadCheckboxes(data, [
                'flipDisplay', 'twelveHourToggle', 'showDayOfWeek', 'showDate',
                'showHumidity', 'colonBlinkEnabled', 'showWeatherDescription',
                'apiEnabled', 'webhooksEnabled', 'authEnabled', 'totpEnabled'
            ]);

            document.getElementById('mdnsEnabled').checked = data.mdnsEnabled !== false;

            document.getElementById('webhookQueueSize').value = data.webhookQueueSize || 5;
            document.getElementById('queueSizeValue').textContent = data.webhookQueueSize || 5;
            document.getElementById('webhookQuietHours').checked = data.webhookQuietHours !== false;

            if (data.webhooksEnabled) {
                document.getElementById('webhookSettings').style.display = 'block';
            }

            document.getElementById('youtubeEnabled').checked = !!(data.youtube && data.youtube.enabled);
            document.getElementById('youtubeApiKey').value = (data.youtube && data.youtube.apiKey) || '';
            document.getElementById('youtubeChannelId').value = (data.youtube && data.youtube.channelId) || '';
            document.getElementById('youtubeShortFormat').checked = data.youtube ? data.youtube.shortFormat : true;

            if (data.authEnabled) {
                document.getElementById('authSettings').style.display = 'block';
            }
            if (data.totpEnabled) {
                document.getElementById('totpSetup').style.display = 'block';
            }

            const dimmingEnabledEl = document.getElementById('dimmingEnabled');
            dimmingEnabledEl.checked = (data.dimmingEnabled === true || data.dimmingEnabled === 'true' || data.dimmingEnabled === 1);

            // Defer field enabling until checkbox state is rendered
            setTimeout(() => {
                setDimmingFieldsEnabled(dimmingEnabledEl.checked);
                setYoutubeFieldsEnabled(document.getElementById('youtubeEnabled').checked);
            }, 0);

            dimmingEnabledEl.addEventListener('change', function () {
                setDimmingFieldsEnabled(this.checked);
            });

            document.getElementById('youtubeEnabled').addEventListener('change', function () {
                setYoutubeEnabled(this.checked);
                setYoutubeFieldsEnabled(this.checked);
            });

            document.getElementById('dimStartTime').value =
                (data.dimStartHour !== undefined ? String(data.dimStartHour).padStart(2, '0') : '18') + ':' +
                (data.dimStartMinute !== undefined ? String(data.dimStartMinute).padStart(2, '0') : '00');

            document.getElementById('dimEndTime').value =
                (data.dimEndHour !== undefined ? String(data.dimEndHour).padStart(2, '0') : '08') + ':' +
                (data.dimEndMinute !== undefined ? String(data.dimEndMinute).padStart(2, '0') : '00');

            document.getElementById('dimBrightness').value = (data.dimBrightness !== undefined ? data.dimBrightness : 2);
            document.getElementById('dimmingBrightnessValue').textContent = (document.getElementById('dimBrightness').value == -1 ? 'Off' : document.getElementById('dimBrightness').value);

            setDimmingFieldsEnabled(!!data.dimmingEnabled);

            document.getElementById('isDramaticCountdown').checked = !!(data.countdown && data.countdown.isDramaticCountdown);
            const countdownEnabledEl = document.getElementById('countdownEnabled');
            countdownEnabledEl.checked = !!(data.countdown && data.countdown.enabled);

            if (data.countdown && data.countdown.targetTimestamp) {
                const targetDate = new Date(data.countdown.targetTimestamp * 1000);
                const year = targetDate.getFullYear();
                const month = (targetDate.getMonth() + 1).toString().padStart(2, '0');
                const day = targetDate.getDate().toString().padStart(2, '0');
                const hours = targetDate.getHours().toString().padStart(2, '0');
                const minutes = targetDate.getMinutes().toString().padStart(2, '0');

                document.getElementById('countdownDate').value = `${year}-${month}-${day}`;
                document.getElementById('countdownTime').value = `${hours}:${minutes}`;
            } else {
                document.getElementById('countdownDate').value = '';
                document.getElementById('countdownTime').value = '';
            }

            const countdownLabelInput = document.getElementById('countdownLabel');
            countdownLabelInput.addEventListener('input', function () {
                this.value = this.value.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
            });

            if (data.countdown && data.countdown.label) {
                countdownLabelInput.value = data.countdown.label.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
            } else {
                countdownLabelInput.value = '';
            }

            countdownEnabledEl.addEventListener('change', function () {
                setCountdownEnabled(this.checked);
                setCountdownFieldsEnabled(this.checked);
            });

            const dramaticCountdownEl = document.getElementById('isDramaticCountdown');
            dramaticCountdownEl.addEventListener('change', function () {
                setIsDramaticCountdown(this.checked);
            });

            setCountdownFieldsEnabled(countdownEnabledEl.checked);

            if (!data.timeZone) {
                try {
                    const tz = Intl.DateTimeFormat().resolvedOptions().timeZone;
                    if (tz && document.getElementById('timeZone').querySelector(`[value="${tz}"]`)) {
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
            showSavingModal('');
            updateSavingModal('⚠️ Failed to load configuration.', false);

            removeReloadButton();
            removeRestoreButton();
            const errorMsg = (err.message || '').toLowerCase();
            if (errorMsg.includes('config corrupted') || errorMsg.includes('failed to write config') || errorMsg.includes('restore')) {
                ensureRestoreButton();
            } else {
                ensureReloadButton();
            }
        });

    document.documentElement.style.height = 'unset';
    document.body.classList.add('loaded');
};

// Form submission
async function submitConfig(event) {
    event.preventDefault();
    if (isSaving) return;
    isSaving = true;

    const form = document.getElementById('configForm');
    const formData = new FormData(form);

    const clockDuration = parseInt(formData.get('clockDuration')) * 1000;
    const weatherDuration = parseInt(formData.get('weatherDuration')) * 1000;
    formData.set('clockDuration', clockDuration);
    formData.set('weatherDuration', weatherDuration);

    formData.set('brightness', document.getElementById('brightnessSlider').value);

    appendCheckboxesToFormData(formData, [
        'flipDisplay', 'twelveHourToggle', 'showDayOfWeek', 'showDate',
        'showHumidity', 'colonBlinkEnabled', 'showWeatherDescription',
        'apiEnabled', 'authEnabled', 'totpEnabled'
    ]);

    formData.set('dimmingEnabled', document.getElementById('dimmingEnabled').checked ? 'true' : 'false');
    const dimStart = document.getElementById('dimStartTime').value;
    const dimEnd = document.getElementById('dimEndTime').value;

    if (dimStart) {
        const [startHour, startMin] = dimStart.split(':').map(x => parseInt(x, 10));
        formData.set('dimStartHour', startHour);
        formData.set('dimStartMinute', startMin);
    }
    if (dimEnd) {
        const [endHour, endMin] = dimEnd.split(':').map(x => parseInt(x, 10));
        formData.set('dimEndHour', endHour);
        formData.set('dimEndMinute', endMin);
    }
    formData.set('dimBrightness', document.getElementById('dimBrightness').value);
    formData.set('weatherUnits', document.getElementById('weatherUnits').checked ? 'imperial' : 'metric');

    formData.set('mdnsEnabled', document.getElementById('mdnsEnabled').checked ? 'true' : 'false');
    formData.set('mdnsHostname', document.getElementById('mdnsHostname').value || 'esptimecast');

    formData.set('countdownEnabled', document.getElementById('countdownEnabled').checked ? 'true' : 'false');
    formData.set('isDramaticCountdown', document.getElementById('isDramaticCountdown').checked ? 'true' : 'false');
    const finalCountdownLabel = document.getElementById('countdownLabel').value.toUpperCase().replace(/[^A-Z0-9 :!'.\-]/g, '');
    formData.set('countdownLabel', finalCountdownLabel);

    appendBoolCheckboxesToFormData(formData, ['webhooksEnabled', 'webhookQuietHours']);
    formData.set('webhookKey', document.getElementById('webhookKey').value);
    formData.set('webhookQueueSize', document.getElementById('webhookQueueSize').value);

    appendBoolCheckboxesToFormData(formData, ['youtubeEnabled', 'youtubeShortFormat']);
    formData.set('youtubeApiKey', document.getElementById('youtubeApiKey').value);
    formData.set('youtubeChannelId', document.getElementById('youtubeChannelId').value);

    formData.set('adminPassword', document.getElementById('adminPassword').value);

    const params = new URLSearchParams();
    for (const pair of formData.entries()) {
        params.append(pair[0], pair[1]);
    }

    showSavingModal('Saving...');

    let currentIsAPMode = false;
    try {
        const apStatusResponse = await fetch('/ap_status');
        const apStatusData = await apStatusResponse.json();
        currentIsAPMode = apStatusData.isAP;
    } catch (error) {
        console.error('Error fetching AP status:', error);
    }

    try {
        const response = await fetch('/save', {
            method: 'POST',
            headers: { 'X-Auth-Token': sessionStorage.getItem('token') || '' },
            body: params
        });

        if (!response.ok) {
            const json = await response.json();
            throw new Error(`Server error ${response.status}: ${json.error}`);
        }

        removeReloadButton();
        removeRestoreButton();

        if (currentIsAPMode) {
            updateSavingModal('✅ Settings saved successfully!<br><br>Rebooting the device now...', false);
            await new Promise(r => setTimeout(r, 5000));
            document.getElementById('configForm').style.display = 'none';
            document.querySelector('.footer').style.display = 'none';
            document.documentElement.style.height = '100vh';
            document.body.style.height = '100vh';
            updateSavingModal('✅ All done!<br>You can now close this tab safely.<br><br>Your device is now rebooting and connecting to your Wi-Fi. Its new IP address will appear on the display for future access.', false);
        } else {
            updateSavingModal('✅ Configuration saved successfully.<br><br>Device will reboot', false);
            setTimeout(() => location.href = location.href.split('#')[0], 3000);
        }
    } catch (err) {
        if (currentIsAPMode && err.message.includes('Failed to fetch')) {
            console.warn('Expected disconnect in AP mode after reboot.');
            showSavingModal('');
            updateSavingModal('✅ Settings saved successfully!<br><br>Rebooting the device now...', false);
            await new Promise(r => setTimeout(r, 5000));
            document.getElementById('configForm').style.display = 'none';
            updateSavingModal('✅ All done!<br>You can now close this tab safely.<br><br>Your device is now rebooting and connecting to your Wi-Fi. Its new IP address will appear on the display for future access.', false);
            removeReloadButton();
            removeRestoreButton();
            return;
        }

        console.error('Save error:', err);
        let friendlyMessage = '⚠️ Something went wrong while saving the configuration.';
        if (err.message.includes('Failed to fetch')) {
            friendlyMessage = '⚠️ Cannot connect to the device.<br>Is it powered on and connected?';
        }

        updateSavingModal(`${friendlyMessage}<br><br>Details: ${err.message}`, false);

        removeReloadButton();
        removeRestoreButton();
        const errorMsg = (err.message || '').toLowerCase();
        if (errorMsg.includes('config corrupted') || errorMsg.includes('failed to write config') || errorMsg.includes('restore')) {
            ensureRestoreButton();
        } else {
            ensureReloadButton();
        }
    } finally {
        isSaving = false;
    }
}

// Modal
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

    if (message.includes('saved successfully') || message.includes('Backup restored') || message.includes('All done!')) {
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
    btn.textContent = options.text || 'Reload Page';
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
    btn.textContent = options.text || 'Restore Backup';
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

async function restoreBackupConfig() {
    showSavingModal('Restoring backup...');
    removeReloadButton();
    removeRestoreButton();

    try {
        const response = await fetch('/restore', {
            method: 'POST',
            headers: { 'X-Auth-Token': sessionStorage.getItem('token') || '' }
        });

        if (!response.ok) {
            throw new Error('Server returned an error');
        }

        updateSavingModal('✅ Backup restored! Device will now reboot.');
        setTimeout(() => location.reload(), 1500);
    } catch (err) {
        console.error('Restore error:', err);
        updateSavingModal(`❌ Failed to restore backup: ${err.message}`, false);
        removeReloadButton();
        removeRestoreButton();
        ensureReloadButton();
    }
}

// Collapsible
const toggle = document.querySelector('.collapsible-toggle');
const content = document.querySelector('.collapsible-content');

toggle.addEventListener('click', function () {
    const isOpen = toggle.classList.toggle('open');
    toggle.setAttribute('aria-expanded', isOpen);
    content.setAttribute('aria-hidden', !isOpen);
    if (isOpen) {
        content.style.height = content.scrollHeight + 'px';
        content.addEventListener('transitionend', function handler() {
            content.style.height = 'auto';
            content.removeEventListener('transitionend', handler);
        });
    } else {
        content.style.height = content.scrollHeight + 'px';
        void content.offsetHeight;
        content.style.height = '0px';
    }
});

// Optional: If open on load, set height to auto
if (toggle.classList.contains('open')) {
    content.style.height = 'auto';
}

// Live settings
let brightnessDebounceTimeout = null;

function setBrightnessLive(val) {
    if (brightnessDebounceTimeout) {
        clearTimeout(brightnessDebounceTimeout);
    }
    brightnessDebounceTimeout = setTimeout(() => {
        postEncodedValue('/set_brightness', val);
    }, 150);
}

function setFlipDisplay(val) { postBoolValue('/set_flip', val); }
function setTwelveHour(val) { postBoolValue('/set_twelvehour', val); }
function setShowDayOfWeek(val) { postBoolValue('/set_dayofweek', val); }
function setShowDate(val) { postBoolValue('/set_showdate', val); }
function setColonBlink(val) { postBoolValue('/set_colon_blink', val); }
function setShowHumidity(val) { postBoolValue('/set_humidity', val); }
function setLanguage(val) { postEncodedValue('/set_language', val); }
function setShowWeatherDescription(val) { postBoolValue('/set_weatherdesc', val); }
function setWeatherUnits(val) { postBoolValue('/set_units', val); }
function setApiEnabled(val) { postBoolValue('/set_api', val); }
function setCountdownEnabled(val) { postBoolValue('/set_countdown_enabled', val); }
function setIsDramaticCountdown(val) { postBoolValue('/set_dramatic_countdown', val); }
function setYoutubeEnabled(val) { postBoolValue('/set_youtube_enabled', val); }
function setYoutubeShortFormat(val) { postBoolValue('/set_youtube_format', val); }

// Countdown
function setCountdownFieldsEnabled(enabled) {
    document.getElementById('countdownLabel').disabled = !enabled;
    document.getElementById('countdownDate').disabled = !enabled;
    document.getElementById('countdownTime').disabled = !enabled;
    document.getElementById('isDramaticCountdown').disabled = !enabled;
}

// Dimming
function setDimmingFieldsEnabled(enabled) {
    document.getElementById('dimStartTime').disabled = !enabled;
    document.getElementById('dimEndTime').disabled = !enabled;
    document.getElementById('dimBrightness').disabled = !enabled;
}

// YouTube
function setYoutubeFieldsEnabled(enabled) {
    document.getElementById('youtubeApiKey').disabled = !enabled;
    document.getElementById('youtubeChannelId').disabled = !enabled;
    document.getElementById('youtubeShortFormat').disabled = !enabled;
    document.getElementById('youtubeSettings').style.display = enabled ? 'block' : 'none';
}

// Webhooks
function setWebhooks(val) {
    postBoolValue('/set_webhooks', val);
    document.getElementById('webhookSettings').style.display = val ? 'block' : 'none';
}

// Geolocation
function getLocation() {
    fetch('http://ip-api.com/json/')
        .then(response => response.json())
        .then(data => {
            document.getElementById('openMeteoLatitude').value = data.lat;
            document.getElementById('openMeteoLongitude').value = data.lon;

            const button = document.getElementById('geo-button');
            let label = data.city || data.regionName || data.country || 'Location Found';

            button.textContent = 'Location: ' + label;
            button.disabled = true;
            button.classList.add('geo-disabled');

            console.log('Location fetched via ip-api.com. Free service: http://ip-api.com/');
        })
        .catch(error => {
            alert(
                'Failed to guess your location.\n\n' +
                'This may happen if:\n' +
                '- You are using an AdBlocker\n' +
                '- There is a network issue\n' +
                '- The service might be temporarily down.\n\n' +
                'You can visit https://www.openstreetmap.org to manually search for your city and get latitude/longitude.'
            );
        });
}
