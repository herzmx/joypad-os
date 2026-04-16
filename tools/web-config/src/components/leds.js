import { DirtyTracker } from './dirty-tracker.js';

/** Feedback Page — LED, Audio, and Rumble configuration */
export class FeedbackCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.visible = false;
        this.currentConfig = null;
    }

    render() {
        this.el.innerHTML = `
            <div class="card" id="feedbackCard" style="display:none;">
                <h2>Feedback</h2>
                <div class="card-content">
                    <h3>LED</h3>
                    <p class="hint">Controls onboard LED (NeoPixel, RGB, or single-color).</p>
                    <div class="pad-form-row">
                        <span class="label">Enable</span>
                        <label class="toggle"><input type="checkbox" id="ledEnable"><span class="toggle-slider"></span></label>
                    </div>
                    <div id="ledSettings" style="display:none;">
                        <div class="pad-form-row">
                            <span class="label">LED Pin</span>
                            <input type="number" id="ledPin" min="0" max="47" value="0">
                            <span class="hint" id="ledPinHint"></span>
                        </div>
                        <div class="pad-form-row">
                            <span class="label">LED Count</span>
                            <input type="number" id="ledCount" min="1" max="16" value="1">
                        </div>
                        <div class="pad-form-row">
                            <span class="label">SInput RGB</span>
                            <label class="toggle"><input type="checkbox" id="sinputRgb"><span class="toggle-slider"></span></label>
                            <span class="hint">Allow host to control RGB LED</span>
                        </div>
                    </div>

                    <h3 style="margin-top: 16px;">Buzzer</h3>
                    <div class="pad-form-row">
                        <span class="label">Enable</span>
                        <label class="toggle"><input type="checkbox" id="speakerEnable"><span class="toggle-slider"></span></label>
                    </div>
                    <div id="speakerSettings" style="display:none;">
                        <div class="pad-form-row">
                            <span class="label">PWM Pin</span>
                            <input type="number" id="speakerPin" min="0" max="47" value="0">
                        </div>
                        <div class="pad-form-row">
                            <span class="label">Enable Pin</span>
                            <input type="number" id="speakerEnablePin" min="-1" max="47" value="-1">
                            <span class="hint">Amplifier shutdown pin (-1 = always on)</span>
                        </div>
                    </div>

                    <div class="buttons" style="margin-top: 12px;">
                        <button id="feedbackSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>`;

        this.el.querySelector('#ledEnable').addEventListener('change', (e) => {
            this.el.querySelector('#ledSettings').style.display = e.target.checked ? '' : 'none';
        });
        this.el.querySelector('#speakerEnable').addEventListener('change', (e) => {
            this.el.querySelector('#speakerSettings').style.display = e.target.checked ? '' : 'none';
        });
        this.el.querySelector('#feedbackSaveBtn').addEventListener('click', () => this.save());
        this.dirty = new DirtyTracker(this.el, this.el.querySelector('#feedbackSaveBtn'));
    }

    async load() {
        const card = this.el.querySelector('#feedbackCard');
        try {
            const config = await this.protocol.getPadConfig();
            if (!config.ok) {
                card.style.display = 'none';
                this.visible = false;
                return;
            }
            card.style.display = '';
            this.visible = true;
            this.currentConfig = config;

            // LED
            const sysPin = config.sys_led_pin;
            const sysCount = config.sys_led_count;
            const savedPin = config.led_pin;
            const savedCount = config.led_count;

            const enableToggle = this.el.querySelector('#ledEnable');
            const settings = this.el.querySelector('#ledSettings');

            if (savedPin !== undefined && savedPin >= 0) {
                enableToggle.checked = true;
                settings.style.display = '';
                this.el.querySelector('#ledPin').value = savedPin;
                this.el.querySelector('#ledCount').value = savedCount || 1;
            } else if (savedPin !== undefined && savedPin < 0) {
                enableToggle.checked = false;
                settings.style.display = 'none';
                this.el.querySelector('#ledPin').value = sysPin >= 0 ? sysPin : 0;
                this.el.querySelector('#ledCount').value = sysCount || 1;
            } else if (sysPin >= 0) {
                enableToggle.checked = true;
                settings.style.display = '';
                this.el.querySelector('#ledPin').value = sysPin;
                this.el.querySelector('#ledCount').value = sysCount || 1;
            } else {
                enableToggle.checked = false;
                settings.style.display = 'none';
            }

            this.el.querySelector('#sinputRgb').checked = config.sinput_rgb || false;

            if (sysPin >= 0) {
                this.el.querySelector('#ledPinHint').textContent = `Default: GPIO ${sysPin}`;
            }

            // Buzzer
            const spkPin = config.speaker_pin !== undefined ? config.speaker_pin : -1;
            const spkEnabled = spkPin > 0;
            this.el.querySelector('#speakerEnable').checked = spkEnabled;
            this.el.querySelector('#speakerSettings').style.display = spkEnabled ? '' : 'none';
            if (spkEnabled) {
                this.el.querySelector('#speakerPin').value = spkPin;
            }
            this.el.querySelector('#speakerEnablePin').value = config.speaker_enable_pin !== undefined ? config.speaker_enable_pin : -1;
            this.dirty?.snapshot();
        } catch (e) {
            card.style.display = 'none';
            this.visible = false;
        }
    }

    async save() {
        if (!confirm('Save feedback configuration? The device will reboot.')) return;
        if (!this.currentConfig) return;

        const config = {
            name: this.currentConfig.name || 'Custom',
            active_high: this.currentConfig.active_high || false,
            i2c_sda: this.currentConfig.i2c_sda !== undefined ? this.currentConfig.i2c_sda : -1,
            i2c_scl: this.currentConfig.i2c_scl !== undefined ? this.currentConfig.i2c_scl : -1,
            deadzone: this.currentConfig.deadzone || 10,
            dpad_mode: this.currentConfig.dpad_mode || 0,
            buttons: this.currentConfig.buttons || [],
            adc: this.currentConfig.adc || [-1, -1, -1, -1, -1, -1],
            invert_lx: this.currentConfig.invert_lx || false,
            invert_ly: this.currentConfig.invert_ly || false,
            invert_rx: this.currentConfig.invert_rx || false,
            invert_ry: this.currentConfig.invert_ry || false,
            sinput_rgb: this.el.querySelector('#sinputRgb').checked,
            led_pin: this.el.querySelector('#ledEnable').checked ? parseInt(this.el.querySelector('#ledPin').value) : -1,
            led_count: this.el.querySelector('#ledEnable').checked ? parseInt(this.el.querySelector('#ledCount').value) : 0,
            speaker_pin: this.el.querySelector('#speakerEnable').checked ? parseInt(this.el.querySelector('#speakerPin').value) : -1,
            speaker_enable_pin: this.el.querySelector('#speakerEnable').checked ? parseInt(this.el.querySelector('#speakerEnablePin').value) : -1,
            usb_host_dp: this.currentConfig.usb_host_dp !== undefined ? this.currentConfig.usb_host_dp : -1,
            ...(() => {
                const tg = {};
                const toggles = this.currentConfig.toggles || [];
                for (let i = 0; i < 2; i++) {
                    const t = toggles[i] || [-1, 0, 0];
                    tg[`toggle${i}_pin`] = t[0];
                    tg[`toggle${i}_func`] = t[1];
                    tg[`toggle${i}_inv`] = t[2];
                }
                return tg;
            })(),
            ...(() => {
                const jw = {};
                const joywing = this.currentConfig.joywing || [];
                for (let i = 0; i < 2; i++) {
                    const slot = joywing[i] || [0, -1, -1, 0x49];
                    jw[`joywing${i}_bus`] = slot[0];
                    jw[`joywing${i}_sda`] = slot[1];
                    jw[`joywing${i}_scl`] = slot[2];
                    jw[`joywing${i}_addr`] = slot[3];
                }
                return jw;
            })(),
        };

        try {
            this.log('Saving feedback config...');
            const result = await this.protocol.setPadConfig(config);
            this.log(result.reboot ? 'Config saved. Device rebooting...' : 'Config saved.', 'success');
        } catch (e) {
            this.log(`Failed to save config: ${e.message}`, 'error');
        }
    }

    isAvailable() { return this.visible; }
}
