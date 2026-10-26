// Run with Node and sharp available (build-time only).
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
(async () => {
  const { data, info } = await sharp(path.join(root, 'data/icons/grid-lock.svg'))
    .resize(14, 14).extend({top: 1, bottom: 1, left: 1, right: 1, background: '#00000000'})
    .ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const rows = Array.from({length: 16}, (_, y) => '  ' + Array.from({length: 16}, (_, x) =>
    String(data[(y * 16 + x) * info.channels + 3]).padStart(3)).join(', ') + ',');
  fs.writeFileSync(path.join(root, 'src/app/ui/oldking/grid_lock_icon.h'),
    '// Generated from data/icons/grid-lock.svg (Lorc, Game-icons.net, CC BY 3.0).\n' +
    '// Regenerate with misc/generate_grid_lock_icon.cjs; no runtime SVG dependency.\n' +
    '#pragma once\nnamespace app { namespace oldking {\n' +
    'constexpr unsigned char kGridLockIconAlpha[256] = {\n' + rows.join('\n') + '\n};\n}}\n');
})();
