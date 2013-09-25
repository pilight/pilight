New (Experimental) Features
=======
- Modular hardware support.
- All function also works with the pilight config.
- Multiple ID's per device (feature request).
- Fixed relay protocol bug in which in detected the wrong `hw-mode`.
- Allow for protocol specific `send_repeats` and `receive_repeats` setting.
- Allow for protocol specific settings to alter default protocol behavior.
- Fixed bugs of the weather module in the webgui.
- Differentiate between internally communicated settings and external ones.

New config syntax:

```
{
	"living": {
		"name": "Living",
		"order": 1,
		"bookshelve": {
			"name": "Book Shelve Light",
			"protocol": "kaku_switch",
			"id": [{
				"id": 1234,
				"unit": 0
			}],
			"state": "off"
		},
		"main": {
			"name": "Main",
			"order": 2,
			"protocol": "kaku_dimmer",
			"type": 1,
			"id": [{
				"id": 1234,
				"unit": 1
			}],
			"state": "on",
			"dimlevel", 0
		},
		"television": {
			"name": "Television",
			"protocol": "relay",
			"id": [{
				"gpio": 3
			}],
			"state": "off"
		} 
	},
	"bedroom": {
		"name": "Bedroom",
		"order": 2,
		"main": {
			"name": "Main",
			"protocol": "elro",
			"id": [{
				"systemcode": 5678,
				"unitcode": 0,
			}],
			"state": "on"
		}
	},
	"garden": {
		"name": "Garden",
		"order": 3,
		"weather": {
			"name": "Weather Station",
			"protocol": "alecto",
			"id": [{
				"id": 100
			}],
			"humidity": 50,
			"temperature": 1530,
			"battery": 1
		}
	}		
}
```
