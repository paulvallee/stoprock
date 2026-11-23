# Case renders

`index.html` draws the two STL files with three.js; `shoot.js` screenshots each view with
Playwright's Chromium. From this folder:

```sh
npm install three@0.170.0 playwright
python3 ../case.py                 # also writes model-*.stl here, in model coordinates
python3 -m http.server 8377 &      # the page loads its modules and the STLs over http
node shoot.js                      # -> view-*.png (front, bezel, right, exploded, tub, mount, lanyard, broom, broom-side)
```

`hardware/case/preview.png` is those five views side by side, with captions.
