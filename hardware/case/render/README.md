# Case renders

`index.html` draws the two STL files with three.js; `shoot.js` screenshots each view with
Playwright's Chromium. From this folder:

```sh
npm install three@0.170.0 playwright
cp ../*.stl .
python3 -m http.server 8377 &      # the page loads its modules and the STLs over http
node shoot.js                      # -> view-front.png, view-back.png, view-tub.png, view-exploded.png, view-right.png
```

`hardware/case/preview.png` is those five views side by side, with captions.
