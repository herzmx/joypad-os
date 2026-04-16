import { DirtyTracker } from './dirty-tracker.js';

/** Router Page — Input routing and D-Pad mode configuration */
export class RouterCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
    }

    render() {
        this.el.innerHTML = `
            <div class="card">
                <h2>Router</h2>
                <div class="card-content">
                    <div class="pad-form-row">
                        <span class="label">Routing Mode</span>
                        <select id="routingMode">
                            <option value="0">Simple (1:1)</option>
                            <option value="1">Merge (N:1)</option>
                            <option value="2" disabled>Broadcast (1:N)</option>
                        </select>
                    </div>
                    <div class="pad-form-row" id="mergeModeRow">
                        <span class="label">Merge Mode</span>
                        <select id="mergeMode">
                            <option value="0">Priority</option>
                            <option value="1">Blend</option>
                            <option value="2">Latest</option>
                        </select>
                    </div>
                    <div class="buttons" style="margin-top: 12px;">
                        <button id="routerSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>

            <div class="card">
                <h2>D-Pad Mode</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Mode</span>
                        <select id="dpadMode">
                            <option value="0">D-Pad</option>
                            <option value="1">Left Stick</option>
                            <option value="2">Right Stick</option>
                        </select>
                    </div>
                    <p class="hint">Maps d-pad buttons to analog stick. Applies to all input sources.</p>
                </div>
            </div>`;

        this.el.querySelector('#routingMode').addEventListener('change', (e) => {
            this.el.querySelector('#mergeModeRow').style.display = e.target.value === '1' ? '' : 'none';
        });
        this.el.querySelector('#routerSaveBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#dpadMode').addEventListener('change', (e) => this.setDpadMode(e.target.value));

        // Dirty tracking — only the routing/merge mode card needs save+reboot
        this.dirty = new DirtyTracker(
            this.el.querySelector('.card'),  // first card (Router)
            this.el.querySelector('#routerSaveBtn')
        );
    }

    async load() {
        try {
            const config = await this.protocol.getRouter();
            this.el.querySelector('#routingMode').value = config.routing_mode || 0;
            this.el.querySelector('#mergeMode').value = config.merge_mode || 0;
            this.el.querySelector('#dpadMode').value = config.dpad_mode || 0;
            this.el.querySelector('#mergeModeRow').style.display =
                (config.routing_mode || 0) === 1 ? '' : 'none';
            this.dirty?.snapshot();
        } catch (e) {
            this.log(`Failed to load router config: ${e.message}`, 'error');
        }
    }

    async save() {
        if (!confirm('Save router configuration? The device will reboot.')) return;

        const config = {
            routing_mode: parseInt(this.el.querySelector('#routingMode').value),
            merge_mode: parseInt(this.el.querySelector('#mergeMode').value),
            dpad_mode: parseInt(this.el.querySelector('#dpadMode').value),
        };

        try {
            this.log('Saving router config...');
            const result = await this.protocol.setRouter(config);
            this.log(result.reboot ? 'Config saved. Device rebooting...' : 'Config saved.', 'success');
        } catch (e) {
            this.log(`Failed to save router config: ${e.message}`, 'error');
        }
    }

    async setDpadMode(mode) {
        try {
            await this.protocol.setDpadMode(parseInt(mode));
            const names = ['D-Pad', 'Left Stick', 'Right Stick'];
            this.log(`D-Pad mode: ${names[parseInt(mode)]}`, 'success');
        } catch (e) {
            this.log(`Failed to set D-Pad mode: ${e.message}`, 'error');
        }
    }
}
