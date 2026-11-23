# Third-party assets

`make assets` (`tools/gen_assets.py`) generates the files in
`components/assets/generated/` from these sources. The firmware embeds the
results.

| Asset | Source | Version | Licence |
| --- | --- | --- | --- |
| Icons (`icons.bin`, `icons_gen.c`) | [Material Design Icons](https://pictogrammers.com/library/mdi/), npm package `@mdi/font` | 7.4.47 | Pictogrammers Free License (icons: Apache License 2.0) |
| Fonts (`font_20.vlw`, `font_28.vlw`, `font_36.vlw`) | [Noto Sans](https://notofonts.github.io/), npm package `@expo-google-fonts/noto-sans` | 0.4.2 | SIL Open Font License 1.1 |
| Time zones (`tz_table_gen.c`) | [posix_tz_db](https://github.com/nayarsystems/posix_tz_db), `zones.json` | commit `fb5fa34` | MIT |

## Material Design Icons

Released by the [Pictogrammers](https://pictogrammers.com/) icon group under
the Pictogrammers Free License. The icons are distributed under the Apache
License, Version 2.0 (https://www.apache.org/licenses/LICENSE-2.0). The
icons are rendered from the package's webfont into bitmaps.

## Noto Sans

Copyright 2022 The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic).
Licensed under the SIL Open Font License, Version 1.1
(https://openfontlicense.org). The fonts are converted to the VLW bitmap
format. The converted fonts are not renamed and are distributed only as part
of the firmware.

## posix_tz_db

Licensed under the MIT License (https://opensource.org/licenses/MIT). The
copyright line in the upstream LICENSE file is an unfilled template
(`[year] [fullname]`); the project is published by
[nayarsystems](https://github.com/nayarsystems).
