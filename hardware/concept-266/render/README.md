# Concept renders

`index.html` draws `../concept.py`'s parts with three.js; `shoot.js` screenshots each view with
Playwright's Chromium. From this folder:

```sh
npm install three@0.170.0
python3 ../concept.py              # -> model-<layout>-*.stl, layouts.json
python3 ../screen_mock.py; python3 ../screen_mock_portrait.py   # -> screen.png, screen-portrait.png
python3 -m http.server 8378 &      # the page loads its modules and the STLs over http
for l in stack row landscape; do node shoot.js $l; done   # -> view-<layout>-*.png
python3 sheet.py                   # -> ../concept-portrait.png, ../concept-landscape.png
```
