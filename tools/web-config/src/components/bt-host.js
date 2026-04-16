/** Bluetooth Host (Input) — Wiimote orientation + BT bonds */
export class BtHostCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.visible = false;
    }

    render() {
        this.el.innerHTML = `
            <div class="card" id="btHostCard" style="display:none;">
                <h2>Bluetooth Host</h2>
                <div class="card-content">
                    <div class="toggle-row" style="margin-bottom: 4px;">
                        <label class="toggle">
                            <input type="checkbox" id="btInputEnable">
                            <span class="toggle-slider"></span>
                        </label>
                        <span>Enable Bluetooth Host</span>
                    </div>
                    <p class="hint" style="margin-bottom: 12px;">Scans for BT/BLE controllers using onboard radio or USB dongle.</p>
                    <div class="row" id="btStatusRow" style="display:none; margin-bottom: 12px;">
                        <span class="label">Status</span>
                        <span class="value" style="display: flex; align-items: center; gap: 8px;">
                            <span class="bt-status-dot" id="btStatusDot"></span>
                            <span id="btStatusText">—</span>
                        </span>
                    </div>
                    <div id="btDevicesList" style="display:none; margin-bottom: 12px;"></div>
                    <div class="pad-form-row">
                        <span class="label">Wiimote Orientation</span>
                        <select id="wiimoteOrientSelect">
                            <option value="0">Auto</option>
                            <option value="1">Horizontal</option>
                            <option value="2">Vertical</option>
                        </select>
                    </div>
                    <p class="hint">Controls D-pad mapping when using Wiimote alone (no extension).</p>
                    <div class="buttons" style="margin-top: 12px;">
                        <button id="btHostSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>
            <div class="card" id="btBondsCard" style="display:none;">
                <h2>Bluetooth Bonds</h2>
                <div class="card-content">
                    <p class="hint">Clear all saved Bluetooth pairings. Devices will need to re-pair.</p>
                    <div>
                        <button class="secondary" id="clearBtBtn">Clear All Bonds</button>
                    </div>
                </div>
            </div>`;

        this.el.querySelector('#btHostSaveBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#clearBtBtn').addEventListener('click', () => this.clearBtBonds());
    }

    async load() {
        await this.loadWiimoteOrient();
        await this.loadBtStatus();
    }

    async loadWiimoteOrient() {
        const card = this.el.querySelector('#btHostCard');
        try {
            const result = await this.protocol.getWiimoteOrient();
            this.el.querySelector('#wiimoteOrientSelect').value = result.mode;
            card.style.display = '';
            this.visible = true;

            // Load BT input toggle from router settings
            try {
                const router = await this.protocol.getRouter();
                this.el.querySelector('#btInputEnable').checked = router.bt_input || false;
            } catch (e) {}
        } catch (e) {
            card.style.display = 'none';
        }
    }

    async loadBtStatus() {
        const card = this.el.querySelector('#btBondsCard');
        try {
            const status = await this.protocol.getBtStatus();
            card.style.display = '';
            this.visible = true;

            const statusRow = this.el.querySelector('#btStatusRow');
            const statusText = this.el.querySelector('#btStatusText');
            const statusDot = this.el.querySelector('#btStatusDot');
            const devicesList = this.el.querySelector('#btDevicesList');

            if (!status.enabled) {
                statusRow.style.display = 'none';
                devicesList.style.display = 'none';
                return;
            }

            statusRow.style.display = '';

            // Color and label based on state
            // Dual-role BLE time-slices scan/advertise — show "Scanning" if enabled with no connections
            statusDot.className = 'bt-status-dot';
            if (status.connections > 0) {
                statusDot.classList.add('connected');
                statusText.textContent = `Connected (${status.connections})`;
            } else {
                statusDot.classList.add('scanning');
                statusText.textContent = 'Scanning';
            }

            // Render device list
            const devices = status.devices || [];
            if (devices.length > 0) {
                devicesList.style.display = '';
                devicesList.innerHTML = devices.map(d => {
                    const meta = d.connected
                        ? `${d.addr} · ${d.ble ? 'BLE' : 'Classic'}${d.vid ? ' · VID:' + d.vid + ' PID:' + d.pid : ''}`
                        : `${d.addr} · ${d.ble ? 'BLE' : 'Classic'} · bonded`;
                    return `
                        <div class="bt-device-row">
                            <span class="bt-status-dot ${d.connected ? 'connected' : 'idle'}"></span>
                            <div style="flex: 1;">
                                <div class="bt-device-name">${d.name || 'Bonded device'}</div>
                                <div class="bt-device-meta">${meta}</div>
                            </div>
                            <button class="secondary bt-forget-btn" data-addr="${d.addr}" style="padding: 4px 10px; font-size: 12px;">Forget</button>
                        </div>
                    `;
                }).join('');

                // Wire up forget buttons
                devicesList.querySelectorAll('.bt-forget-btn').forEach(btn => {
                    btn.addEventListener('click', () => this.forgetDevice(btn.dataset.addr));
                });
            } else {
                devicesList.style.display = 'none';
            }

            // Auto-refresh while page is visible
            if (!this._statusInterval) {
                this._statusInterval = setInterval(() => this.loadBtStatus(), 2000);
            }
        } catch (e) {
            card.style.display = 'none';
        }
    }

    async save() {
        if (!confirm('Save Bluetooth host configuration? The device will reboot.')) return;
        try {
            // Save Wiimote orientation
            const mode = parseInt(this.el.querySelector('#wiimoteOrientSelect').value);
            await this.protocol.setWiimoteOrient(mode);

            // Save BT input toggle via router settings
            const router = await this.protocol.getRouter();
            await this.protocol.setRouter({
                routing_mode: router.routing_mode || 0,
                merge_mode: router.merge_mode || 0,
                dpad_mode: router.dpad_mode || 0,
                bt_input: this.el.querySelector('#btInputEnable').checked,
            });
        } catch (e) {
            this.log(`Failed to save: ${e.message}`, 'error');
        }
    }

    async clearBtBonds() {
        if (!confirm('Clear all Bluetooth bonds? Devices will need to re-pair.')) return;
        try {
            await this.protocol.clearBtBonds();
            this.log('Bluetooth bonds cleared', 'success');
        } catch (e) {
            this.log(`Failed to clear bonds: ${e.message}`, 'error');
        }
    }

    async forgetDevice(addr) {
        if (!confirm(`Forget device ${addr}? It will be disconnected and bond removed.`)) return;
        try {
            await this.protocol.forgetBtDevice(addr);
            this.log(`Forgot device ${addr}`, 'success');
            await this.loadBtStatus();
        } catch (e) {
            this.log(`Failed to forget device: ${e.message}`, 'error');
        }
    }

    isAvailable() { return this.visible; }
}
