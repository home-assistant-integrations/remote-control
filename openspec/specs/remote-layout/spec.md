# remote-layout Specification

## Purpose

How a Home Assistant dashboard becomes remotes: views as remotes, which
cards are read, the supported Bubble Card types and button properties,
limits, and reloading when the dashboard changes.

## Requirements

### Requirement: Views are remotes

The device SHALL read the dashboard `home_assistant.dashboard` with
`lovelace/config`. Every view in `views[]` is one remote, in the dashboard's
order. A remote's title is the view's `title`, or else its `path`, or else
"Remote N".

#### Scenario: One view

- **WHEN** the dashboard has one view titled "Wohnzimmer"
- **THEN** there is exactly one remote, titled "Wohnzimmer"

#### Scenario: Three views

- **WHEN** the dashboard has three views
- **THEN** there are three remotes, in the order of the views

#### Scenario: Dashboard missing

- **WHEN** `lovelace/config` fails, for example with `config_not_found`
- **THEN** the UI shows "dashboard not found" with the configured name

### Requirement: Card discovery

For each view, the device SHALL collect cards in document order from
`views[].cards[]` (masonry view) and from `views[].sections[].cards[]`
(sections view). Cards of any other type than the supported Bubble Card types
MUST be skipped and logged by type.

#### Scenario: Sections view

- **WHEN** a view has `"type":"sections"` with cards inside `sections[]`
- **THEN** all of those cards are collected in order

#### Scenario: Unsupported card

- **WHEN** a view contains an `entities` card
- **THEN** that card is skipped, the log names its type, and the rest of the
  view is still shown

### Requirement: Supported Bubble Card types

The device SHALL turn these `custom:bubble-card` card types into layout
elements:

| `card_type` | Element |
| --- | --- |
| `separator` | Heading with `icon` and `name` |
| `button` | One button with `name`, `icon`, `entity` and its actions |
| `media-player` | Media tile for `entity` with `name` and `icon`, plus the implicit controls power, previous, play/pause and next, and volume down/up unless `hide.volume_button` is true. A control is left out if `hide.<control>_button` is true |
| `sub-buttons` | One row per entry of `sub_button.bottom[]`, each with the buttons in its `group[]` |

#### Scenario: Your remote

- **WHEN** the layout of the `media-control` dashboard is read
- **THEN** it has a heading "Apple TV Wohnzimmer", a media tile for
  `media_player.wohnzimmer_apple_tv_wohnzimmer` with power, previous,
  play/pause and next but no volume controls, and five button rows with 1, 3,
  1, 2 and 2 buttons

### Requirement: Button properties

For every button, the device SHALL take:

- `name`, and `icon` as a Material Design Icon name (`mdi:…`)
- `show_icon` and `show_name`, both defaulting to true
- `width` and `custom_height` in CSS pixels when given
- the row's `justify_content`: `center` keeps the given widths, `fill`
  divides the row width evenly
- `tap_action`, `hold_action` and the card's `entity`

#### Scenario: OK button without icon

- **WHEN** a button has `"show_icon": false, "show_name": true`
- **THEN** its element shows only the name

#### Scenario: Fill row

- **WHEN** a row with `"justify_content":"fill"` has two buttons
- **THEN** each button gets half of the row width, minus spacing

### Requirement: Layout reload on change

The device SHALL subscribe to `lovelace_updated` events. When an event names
the configured dashboard, it MUST read the configuration again and rebuild the
remotes. It keeps the current page if a page with the same index still
exists, and otherwise goes to the first page.

#### Scenario: Button added in Home Assistant

- **WHEN** the user adds a button to the dashboard in Home Assistant and saves
- **THEN** the device shows the new button within 5 seconds, without a restart

### Requirement: Layout limits

The parser SHALL accept up to 16 remotes per dashboard and 64 buttons per
remote. It MUST ignore and log anything beyond these limits instead of
failing.

#### Scenario: Too many buttons

- **WHEN** a view has 80 buttons
- **THEN** the first 64 are shown and the log reports that 16 were left out
