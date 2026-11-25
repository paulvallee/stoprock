# Concept renders

`index.html` draws `../concept.py`'s parts with three.js; `shoot.js` screenshots each view with
Playwright's Chromium. From this folder:

```sh
npm install three@0.170.0
python3 ../concept.py              # -> model-*.stl
python3 ../screen_mock.py          # -> screen.png (the rough 296 x 152 screen)
python3 -m http.server 8378 &      # the page loads its modules and the STLs over http
node shoot.js                      # -> view-*.png
python3 sheet.py                   # -> ../concept.png
```
