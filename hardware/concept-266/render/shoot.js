// Screenshots each view of index.html (serve this folder over http on port 8377 first).
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await b.newPage({ viewport: { width: 640, height: 560 }, deviceScaleFactor: 2 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e)));
  page.on('console', m => { if (m.type() === 'error') errs.push(m.text()); });
  const layout = process.argv[2] || 'stack';
  const views = process.argv.slice(3).length ? process.argv.slice(3)
    : ["front", "face", "broom", "broom-hand", "side", "back"];
  for (const v of views) {
    await page.goto(`http://127.0.0.1:8378/index.html?layout=${layout}&view=${v}`);
    await page.waitForFunction(() => document.title === 'done', null, { timeout: 60000 });
    await page.screenshot({ path: `view-${layout}-${v}.png` });
  }
  console.log('errors:', JSON.stringify(errs));
  await b.close();
})();
