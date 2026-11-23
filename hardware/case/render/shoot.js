const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await b.newPage({ viewport: { width: 640, height: 560 }, deviceScaleFactor: 2 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e)));
  page.on('console', m => { if (m.type() === 'error') errs.push(m.text()); });
  for (const v of ['front', 'back', 'tub', 'exploded', 'right']) {
    await page.goto(`http://127.0.0.1:8377/index.html?view=${v}`);
    await page.waitForFunction(() => document.title === 'done', null, { timeout: 30000 });
    await page.screenshot({ path: `view-${v}.png` });
  }
  console.log('errors:', JSON.stringify(errs));
  await b.close();
})();
