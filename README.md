New (Experimental) Features
=======
- Modular hardware support
- All function also works with the pilight config
- Multiple ID's per device (feature request)
- Fixed relay protocol bug in which in detected the wrong `hw-mode`
- Allow for protocol specific `send_repeats` and `receive_repeats` setting.

New config syntax:

```
{
	"living": {
		"name": "Living",
		"order": 1,
		"bookshelve": {
			"name": "Book Shelve Light",
			"order": 1,
			"protocol": "kaku_switch",
			"type": 1,
			"id": [{
				"id": 1234,
				"unit": 0
			}],
			"state": "off",
			"values": [ "on", "off" ]
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
			"dimlevel", 0,
			"values": [ "on", "off" ]
		},
		"television": {
			"name": "Television",
			"order": 3,
			"protocol": "relay",
			"type": 1,
			"id": [{
				"gpio": 3
			}],
			"state": "off",
			"values": [ "on", "off" ]
		} 
	},
	"bedroom": {
		"name": "Bedroom",
		"order": 2,
		"main": {
			"name": "Main",
			"order": 1,
			"protocol": "elro",
			"type": 1,
			"id": [{
				"systemcode": 5678,
				"unitcode": 0,
			}],
			"state": "on",
			"values": [ "on", "off" ]
		}
	},
	"garden": {
		"name": "Garden",
		"order": 3,
		"weather": {
			"name": "Weather Station",
			"order": 1,
			"protocol": "alecto",
			"type": 3,
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
