var oWebsocket = false;
var bForceAjax = false;
var bConnected = false;
var bInitialized = false;
var bSending = false;
var aDecimals = new Array();
var aDateTime = new Array();
var aMinInterval = new Array();
var aPollInterval = new Array();
var aReadOnly = new Array();
var bShowTabs = true;
var iPLVersion = 0;
var iPLNVersion = 0;
var iFWVersion = 0;
var aTimers = new Array();
var sDateTimeFormat = "HH:mm:ss YYYY-MM-DD";
var aDateTimeFormats = new Array();
var userLang = navigator.language || navigator.userLanguage;

var language_en = {
	off: "Off",
	on: "On",
	stopped: "Stopped",
	started: "Started",
	toggling: "Toggling",
	up: "Up",
	down: "Down",
	update: "Update",
	loading: "Loading",
	available: "available",
	connecting: "Connecting",
	connection_lost: "Connection lost, touch to reload",
	connection_failed: "Failed to connect, touch to reload",
	unexpected_error: "An unexpected error occured"
}

var language_nl = {
	off: "Uit",
	on: "Aan",
	stopped: "Gestopt",
	started: "Gestart",
	toggling: "Omzetten",
	up: "Omhoog",
	down: "Omlaag",
	update: "Bijwerken",
	loading: "Verbinding maken",
	available: "beschikbaar",
	connection_lost: "Verbinding verloren, klik om te herladen",
	connection_failed: "Kan niet verbinden, klik om te herhalen",
	unexpected_error: "An unexpected error occured"
}

var language = language_en;

if(userLang.indexOf('nl') != -1) {
	language = language_nl;
}

var cookieEnabled = (navigator.cookieEnabled) ? true : false;

if(typeof navigator.cookieEnabled == "undefined" && !cookieEnabled) {
	document.cookie="testcookie";
	cookieEnabled = (document.cookie.indexOf("testcookie") != -1) ? true : false;
	document.cookie = 'testcookie=; expires=Thu, 01 Jan 1970 00:00:01 GMT;';
}

function toggleTabs() {
	if(cookieEnabled && typeof(localStorage) != 'undefined') {
		try {
			if(localStorage.getItem("tabs") == "true") {
				localStorage.setItem("tabs", "false");
			} else {
				localStorage.setItem("tabs", "true");
			}
		} catch (e) {
			if (e == QUOTA_EXCEEDED_ERR) {
				alert('Quota exceeded!');
			}
		}
		document.location = document.location;
	}
}

if(cookieEnabled && typeof(localStorage) != 'undefined') {
	var iLatestTap1 = 0;
	var iLatestTap2 = 0;

	if(localStorage.getItem("tabs") === null) {
		localStorage.setItem("tabs", "true");
	}

	if(localStorage.getItem("tabs") == "false") {
		bShowTabs = false;
	}

	$(document).click(function(e) {
		if($('#helpdlg').length == 1) {
			$('#helpdlg').remove();
		}
	});

	$(document).mouseup(function(e) {
		if(e.target.className.indexOf('ui-page') == 0 || e.target.className.indexOf('ui-content') == 0) {
			iDate = new Date().getTime();
			if((iLatestTap1-iDate) > 2000) {
				iLatestTap1 = 0;
				iLatestTap2 = 0;
			}
			if(iLatestTap1 == 0) {
				iLatestTap1=new Date().getTime();
			} else if(iLatestTap2 == 0) {
				iLatestTap2=new Date().getTime();
				if((iLatestTap2-iLatestTap1) > 250 && (iLatestTap2-iLatestTap1) < 1000) {
					toggleTabs();
				}
				iLatestTap1 = 0;
				iLatestTap2 = 0;
			}
		}
	});

	$(document).keydown(function(e) {
		if(e.keyCode == 72) {
			if($('#helpdlg').length == 1) {
				$('#helpdlg').remove();
			} else {
				$("<div data-theme='a' id='helpdlg' class='ui-loader ui-overlay-shadow ui-body-e ui-corner-all'><b>Short Cuts</b><br /><ul><li>[ctrl] + [alt] + [shift] + t<br />Switch between tabular or vertical view</li></ul></div>")
				.appendTo($('body'));
				$('#helpdlg').css({ "display": "block", "padding": "10px", "width": "300px", "position": "relative", "left" : ($(window).width() / 2)-150+'px', "top" : (($(window).height() / 2)-($('#helpdlg').height() / 2))+'px'})
			}
		} else {
			if($('#helpdlg').length == 1) {
				$('#helpdlg').remove();
			}
		}
		if(e.keyCode == 84 && e.altKey && e.ctrlKey && e.shiftKey) {
			toggleTabs();
		}
	});
}

function createSwitchElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_switch').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li id="'+sTabId+'_'+sDevId+'" class="switch" data-icon="false"><div class="name">'+aValues['name']+'</div><select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">'+language.off+'</option><option value="on">'+language.on+'</option></select></li>'));
		}
		$('#'+sTabId+'_'+sDevId+'_switch').slider();
		$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
			event.stopPropagation();
			if('all' in aValues && aValues['all'] == 1) {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'","values":{"all": 1}}}';
			} else {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
			}
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});
		oTab.listview();
		oTab.listview("refresh");
	}
	if('state' in aValues) {
		if(aValues['state'] === "on" || aValues['state'] === "opened") {
			$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
			$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
		} else {
			$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 0;
			$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
		}
	}
	if('gui-readonly' in aValues && aValues['gui-readonly']) {
		aReadOnly[sTabId+'_'+sDevId] = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('disable');
	} else {
		aReadOnly[sTabId+'_'+sDevId] = 0;
	}
}

function createPendingSwitchElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_pendingsw').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}

		if('name' in aValues) {
			oTab.append($('<li id="'+sTabId+'_'+sDevId+'" class="pendingsw" data-icon="false"><div class="name">'+aValues['name']+'</div><a data-role="button" data-inline="true" data-mini="true" data-icon="on" id="'+sTabId+'_'+sDevId+'_pendingsw">&nbsp;</a></li>'));
		}
		$('#'+sTabId+'_'+sDevId+'_pendingsw').button();
		$('#'+sTabId+'_'+sDevId+'_pendingsw').bind("click", function(event, ui) {
			event.stopPropagation();
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().removeClass('ui-icon-on').removeClass('ui-icon-off').addClass('ui-icon-loader');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('disable');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').text(language.toggling);
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('refresh');
			var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});
		$('#'+sTabId+'_'+sDevId).bind("click", function(event, ui) {
			if(!$('#'+sTabId+'_'+sDevId+'_pendingsw').prop("disabled")) {
				event.stopPropagation();
				$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().removeClass('ui-icon-on').removeClass('ui-icon-off').addClass('ui-icon-loader');
				$('#'+sTabId+'_'+sDevId+'_pendingsw').button('disable');
				$('#'+sTabId+'_'+sDevId+'_pendingsw').text(language.toggling);
				$('#'+sTabId+'_'+sDevId+'_pendingsw').button('refresh');
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
				if(oWebsocket) {
					oWebsocket.send(json);
				} else {
					bSending = true;
					$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
					window.setTimeout(function() { bSending = false; }, 1000);
				}
			}
		});
		oTab.listview();
		oTab.listview("refresh");
	}
	if('state' in aValues) {
		if($('#'+sTabId+'_'+sDevId+'_pendingsw').parent().attr("class").indexOf("ui-icon-loader") >= 0) {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().removeClass('ui-icon-loader');
		}
		if($('#'+sTabId+'_'+sDevId+'_pendingsw').parent().attr("class").indexOf("ui-icon-on") >= 0) {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().removeClass('ui-icon-on');
		}
		if($('#'+sTabId+'_'+sDevId+'_pendingsw').parent().attr("class").indexOf("ui-icon-off") >= 0) {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().removeClass('ui-icon-off');
		}

		if(aValues['state'] == "running") {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().addClass('ui-icon-on');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('enable');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').text(language.started);
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('refresh');
		} else if(aValues['state'] == "pending") {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().addClass('ui-icon-loader');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('disable');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').text(language.toggling);
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('refresh');
		} else {
			$('#'+sTabId+'_'+sDevId+'_pendingsw').parent().addClass('ui-icon-off');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').text(language.stopped);
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('enable');
			$('#'+sTabId+'_'+sDevId+'_pendingsw').button('refresh');
		}
	}
	if('gui-readonly' in aValues && aValues['gui-readonly']) {
		aReadOnly[sTabId+'_'+sDevId] = 1;
		$('#'+sTabId+'_'+sDevId+'_pendingsw').button('disable');
	} else {
		aReadOnly[sTabId+'_'+sDevId] = 0;
	}
}

function createScreenElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_screen').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li  id="'+sTabId+'_'+sDevId+'" class="screen" data-icon="false"><div class="name">'+aValues['name']+'</div><div id="'+sTabId+'_'+sDevId+'_screen" class="screen" data-role="fieldcontain" data-type="horizontal"><fieldset data-role="controlgroup" class="controlgroup" data-type="horizontal" data-mini="true"><input type="radio" name="'+sTabId+'_'+sDevId+'_screen" id="'+sTabId+'_'+sDevId+'_screen_down" value="down" /><label for="'+sTabId+'_'+sDevId+'_screen_down">'+language.down+'</label><input type="radio" name="'+sTabId+'_'+sDevId+'_screen" id="'+sTabId+'_'+sDevId+'_screen_up" value="up" /><label for="'+sTabId+'_'+sDevId+'_screen_up">'+language.up+'</label></fieldset></div></li>'));
		}
		$("div").trigger("create");
		$('#'+sTabId+'_'+sDevId+'_screen_down').checkboxradio();
		$('#'+sTabId+'_'+sDevId+'_screen_up').checkboxradio();
		$('#'+sTabId+'_'+sDevId+'_screen_down').bind("change", function(event, ui) {
			event.stopPropagation();
			i = 0;
			oLabel = this.parentNode.getElementsByTagName('label')[0];
			$(oLabel).removeClass('ui-btn-active');
			x = window.setInterval(function() {
				i++;
				if(i%2 == 1)
					$(oLabel).removeClass('ui-btn-active');
				else
					$(oLabel).addClass('ui-btn-active');
				if(i==2)
					window.clearInterval(x);
			}, 100);
			if('all' in aValues && aValues['all'] == 1) {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'","values":{"all": 1}}}';
			} else {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}'
			}
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});
		$('#'+sTabId+'_'+sDevId+'_screen_up').bind("change", function(event, ui) {
			event.stopPropagation();
			i = 0;
			oLabel = this.parentNode.getElementsByTagName('label')[0];
			$(oLabel).removeClass('ui-btn-active');
			x = window.setInterval(function() {
				i++;
				if(i%2 == 1)
					$(oLabel).removeClass('ui-btn-active');
				else
					$(oLabel).addClass('ui-btn-active');
				if(i==2)
					window.clearInterval(x);
			}, 100);
			if('all' in aValues && aValues['all'] == 1) {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'","values":{"all": 1}}}';
			} else {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}'
			}
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});
	}
	if('state' in aValues) {
		if(aValues['state'] == "up") {
			$('#'+sTabId+'_'+sDevId+'_screen_up').attr("checked","checked")
			$('#'+sTabId+'_'+sDevId+'_screen_up').checkboxradio("refresh");
		} else {
			$('#'+sTabId+'_'+sDevId+'_screen_down').attr("checked","checked")
			$('#'+sTabId+'_'+sDevId+'_screen_down').checkboxradio("refresh");
		}
	}
	oTab.listview();
	oTab.listview("refresh");
	if('gui-readonly' in aValues && aValues['gui-readonly']) {
		aReadOnly[sTabId+'_'+sDevId] = 1;
		$('#'+sTabId+'_'+sDevId+'_screen_up').checkboxradio('disable');
		$('#'+sTabId+'_'+sDevId+'_screen_down').checkboxradio('disable');
	} else {
		aReadOnly[sTabId+'_'+sDevId] = 0;
	}
}

function createDimmerElement(sTabId, sDevId, aValues) {
	if('dimlevel' in aValues) {
		iOldDimLevel = aValues['dimlevel'];
	} else {
		iOldDimLevel = 0;
	}
	if($('#'+sTabId+'_'+sDevId+'_switch').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues && 'dimlevel-minimum' in aValues && 'dimlevel-maximum' in aValues) {
			oTab.append($('<li id="'+sTabId+'_'+sDevId+'" class="dimmer" data-icon="false"><div class="name">'+aValues['name']+'</div><select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">'+language.off+'</option><option value="on">'+language.on+'</option></select><div id="'+sTabId+'_'+sDevId+'_dimmer" min="'+aValues['dimlevel-minimum']+'" max="'+aValues['dimlevel-maximum']+'" data-highlight="true" ><input type="value" id="'+sTabId+'_'+sDevId+'_value" class="slider-value dimmer-slider ui-slider-input ui-input-text ui-body-c ui-corner-all ui-shadow-inset" /></div></li>'));
		}
		$('#'+sTabId+'_'+sDevId+'_switch').slider();
		$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
			event.stopPropagation();
			if('all' in aValues && aValues['all'] == 1) {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'","values":{"all": 1}}}';
			} else {
				var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
			}
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});

		$('#'+sTabId+'_'+sDevId+'_dimmer').slider({
			stop: function() {
				if(iOldDimLevel != this.value) {
					iOldDimLevel = this.value;
					$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
					$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
					var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"on","values":{"dimlevel":'+this.value+'}}}';
					if(oWebsocket) {
						oWebsocket.send(json);
					} else {
						bSending = true;
						$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
						window.setTimeout(function() { bSending = false; }, 1000);
					}
				}
			}
		});
		$('#'+sTabId+'_'+sDevId+'_dimmer').change(function(event) {
			$('#'+sTabId+'_'+sDevId+'_value').val(this.value);
		});

		oTab.listview();
		oTab.listview("refresh");
	}
	$('#'+sTabId+'_'+sDevId+'_dimmer').val(iOldDimLevel);
	$('#'+sTabId+'_'+sDevId+'_dimmer').slider('refresh');
	if('state' in aValues) {
		if(aValues['state'] == "on") {
			$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
			$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
		} else {
			$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 0;
			$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
		}
	}
	if('gui-readonly' in aValues && aValues['gui-readonly']) {
		aReadOnly[sTabId+'_'+sDevId] = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('disable');
		$('#'+sTabId+'_'+sDevId+'_dimmer').slider('disable');
	} else {
		aReadOnly[sTabId+'_'+sDevId] = 0;
	}
}

function createWeatherElement(sTabId, sDevId, aValues) {
	aDecimals[sTabId+'_'+sDevId] = new Array();
	aMinInterval[sTabId+'_'+sDevId] = new Array();
	aPollInterval[sTabId+'_'+sDevId] = new Array();
	if('gui-decimals' in aValues) {
		aDecimals[sTabId+'_'+sDevId]['gui'] = aValues['gui-decimals'];
	} else {
		aDecimals[sTabId+'_'+sDevId]['gui'] = 0;
	}
	if('device-decimals' in aValues) {
		aDecimals[sTabId+'_'+sDevId]['device'] = aValues['device-decimals'];
	} else {
		aDecimals[sTabId+'_'+sDevId]['device'] = 0;
	}
	if('min-interval' in aValues) {
		aMinInterval[sTabId+'_'+sDevId] = aValues['min-interval'];
	}
	if('poll-interval' in aValues) {
		aPollInterval[sTabId+'_'+sDevId] = aValues['poll-interval'];
	}
	if('temperature' in aValues) {
		aValues['temperature'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('humidity' in aValues) {
		aValues['humidity'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('rain' in aValues) {
		aValues['rain'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('sunrise' in aValues) {
		aValues['sunrise'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('sunset' in aValues) {
		aValues['sunset'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('windavg' in aValues) {
		aValues['windavg'] /= Math.pow(10, aValues['device-decimals']);
	}
	if('windgust' in aValues) {
		aValues['windgust'] /= Math.pow(10, aValues['device-decimals']);
	}

	if($('#'+sTabId+'_'+sDevId+'_weather').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li class="weather" id="'+sTabId+'_'+sDevId+'_weather" data-icon="false"><div class="name">'+aValues['name']+'</div></li>'));
		}
		if('gui-show-update' in aValues && aValues['gui-show-update']) {
			iTime = Math.floor((new Date().getTime())/1000);

			if('timestamp' in aValues && 'min-interval' in aValues && aValues['timestamp'] > 0 && (iTime-aValues['timestamp']) > aValues['min-interval']) {
				oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="update_active" id="'+sTabId+'_'+sDevId+'_upd" title="'+language.update+'">&nbsp;</div>'));
			} else {
				oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="update_inactive" id="'+sTabId+'_'+sDevId+'_upd" title="'+language.update+'">&nbsp;</div>'));
			}
			$('#'+sTabId+'_'+sDevId+'_upd').click(function() {
				if(this.className.indexOf('update_active') == 0) {
					var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","values":{"update":1}}}';
					if(oWebsocket) {
						oWebsocket.send(json);
					} else {
						bSending = true;
						$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
						window.setTimeout(function() { bSending = false; }, 1000);
					}
					$('#'+sTabId+'_'+sDevId+'_upd').removeClass('update_active').addClass('update_inactive');
					iTimeOut = aValues['min-interval']*1000;
					window.clearTimeout(aTimers[sTabId+'_'+sDevId]);
					aTimers[sTabId+'_'+sDevId] = window.setTimeout(function() {
						if($('#'+sTabId+'_'+sDevId+'_upd').attr("class").indexOf('update_inactive') != -1) {
							$('#'+sTabId+'_'+sDevId+'_upd').removeClass('update_inactive').addClass('update_active');
						}
					}, iTimeOut);
				}
			});
			if(aValues['timestamp'] > 0) {
				iTimeOut = (aValues['min-interval']-((iTime-aValues['timestamp'])))*1000;
				window.clearTimeout(aTimers[sTabId+'_'+sDevId]);
				aTimers[sTabId+'_'+sDevId] = window.setTimeout(function() {
					if($('#'+sTabId+'_'+sDevId+'_upd').attr("class").indexOf('update_inactive') != -1) {
						$('#'+sTabId+'_'+sDevId+'_upd').removeClass('update_inactive').addClass('update_active');
					}
				}, iTimeOut);
			}
		}
		if('gui-show-battery' in aValues && aValues['gui-show-battery'] && 'battery' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div id="'+sTabId+'_'+sDevId+'_batt" class="battery"></div>'));
			if('battery' in aValues) {
				if(aValues['battery']) {
					$('#'+sTabId+'_'+sDevId+'_batt').addClass('green');
				} else {
					$('#'+sTabId+'_'+sDevId+'_batt').addClass('red');
				}
			}
		}
		if('gui-show-rain' in aValues && aValues['gui-show-rain'] && 'rain' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="rain_icon"></div><div class="rain" id="'+sTabId+'_'+sDevId+'_rain">'+aValues['rain'].toFixed(aValues['gui-decimals'])+'</div>'));
		}
		if('gui-show-wind' in aValues && aValues['gui-show-wind']) {
			if('windavg' in aValues) {
				oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="windavg_icon"></div><div class="windavg" id="'+sTabId+'_'+sDevId+'_windavg">'+aValues['windavg']+'</div>'));
			}
			if('windgust' in aValues) {
				oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="windgust_icon"></div><div class="winddir_icon"></div><div class="windgust" id="'+sTabId+'_'+sDevId+'_windgust">'+aValues['windgust']+'</div>'));
			}
			if('winddir' in aValues) {
				$('#'+sTabId+'_'+sDevId+'_weather .winddir_icon').css({transform: 'rotate(' + aValues['winddir'] + 'deg)'});
			}
		}
		if('gui-show-humidity' in aValues && aValues['gui-show-humidity'] && 'humidity' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="humidity_icon"></div><div class="humidity" id="'+sTabId+'_'+sDevId+'_humi">'+aValues['humidity'].toFixed(aValues['gui-decimals'])+'</div>'));
		}
		if('gui-show-temperature' in aValues && aValues['gui-show-temperature'] && 'temperature' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="temperature_icon"></div><div class="temperature" id="'+sTabId+'_'+sDevId+'_temp">'+aValues['temperature'].toFixed(aValues['gui-decimals'])+'</div>'));
		}
		if('gui-show-sunriseset' in aValues && aValues['gui-show-sunriseset'] && 'sunrise' in aValues && 'sunset' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div id="'+sTabId+'_'+sDevId+'_sunset_icon" class="sunset_icon"></div><div class="sunset" id="'+sTabId+'_'+sDevId+'_sunset">'+aValues['sunset'].toFixed(aValues['gui-decimals'])+'</div>'));
			oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div id="'+sTabId+'_'+sDevId+'_sunrise_icon" class="sunrise_icon"></div><div class="sunrise" id="'+sTabId+'_'+sDevId+'_sunrise">'+aValues['sunrise'].toFixed(aValues['gui-decimals'])+'</div>'));
			if('sun' in aValues) {
				if(aValues['sun'] == 'rise') {
					$('#'+sTabId+'_'+sDevId+'_sunrise_icon').addClass('yellow');
					$('#'+sTabId+'_'+sDevId+'_sunset_icon').addClass('gray');
				} else {
					$('#'+sTabId+'_'+sDevId+'_sunrise_icon').addClass('gray');
					$('#'+sTabId+'_'+sDevId+'_sunset_icon').addClass('blue');
				}
			}
		}
	} else {
		if('gui-show-battery' in aValues && aValues['gui-show-battery']) {
			if(aValues['battery']) {
				if($('#'+sTabId+'_'+sDevId+'_batt').attr("class").indexOf("green") == -1) {
					$('#'+sTabId+'_'+sDevId+'_batt').removeClass('red').addClass('green');
				}
			} else {
				if($('#'+sTabId+'_'+sDevId+'_batt').attr("class").indexOf("red") == -1) {
					$('#'+sTabId+'_'+sDevId+'_batt').removeClass('green').addClass('red');
				}
			}
		}
		if('sun' in aValues && $('#'+sTabId+'_'+sDevId+'_sunrise_icon').attr("class")
		   && $('#'+sTabId+'_'+sDevId+'_sunset_icon').attr("class")) {
			if(aValues['sun'] == 'rise') {
				if($('#'+sTabId+'_'+sDevId+'_sunrise_icon').attr("class").indexOf("yellow") == -1) {
					$('#'+sTabId+'_'+sDevId+'_sunrise_icon').removeClass('gray').addClass('yellow');
				}
				if($('#'+sTabId+'_'+sDevId+'_sunrise_icon').attr("class").indexOf("gray") == -1) {
					$('#'+sTabId+'_'+sDevId+'_sunset_icon').removeClass('blue').addClass('gray');
				}
			} else {
				if($('#'+sTabId+'_'+sDevId+'_sunrise_icon').attr("class").indexOf("gray") == -1) {
					$('#'+sTabId+'_'+sDevId+'_sunrise_icon').removeClass('yellow').addClass('gray');
				}
				if($('#'+sTabId+'_'+sDevId+'_sunrise_icon').attr("class").indexOf("blue") == -1) {
					$('#'+sTabId+'_'+sDevId+'_sunset_icon').removeClass('gray').addClass('blue');
				}
			}
		}
		if('gui-show-humidity' in aValues && aValues['gui-show-humidity'] && 'humidity' in aValues) {
			$('#'+sTabId+'_'+sDevId+'_humi').text(aValues['humidity'].toFixed(aValues['gui-decimals']));
		}
		if('gui-show-rain' in aValues && aValues['gui-show-rain'] && 'rain' in aValues) {
			$('#'+sTabId+'_'+sDevId+'_rain').text(aValues['rain'].toFixed(aValues['gui-decimals']));
		}
		if('gui-show-wind' in aValues && aValues['gui-show-wind']) {
			if('windavg' in aValues) {
				$('#'+sTabId+'_'+sDevId+'_windavg').text(aValues['windavg'].toFixed(aValues['gui-decimals']));
			}
			if('winddir' in aValues) {
				$('#'+sTabId+'_'+sDevId+'_weather .winddir_icon').css({transform: 'rotate(' + aValues['winddir'] + 'deg)'});
			}
			if('windgust' in aValues) {
				$('#'+sTabId+'_'+sDevId+'_windgust').text(aValues['windgust'].toFixed(aValues['gui-decimals']));
			}
		}
		if('gui-show-temperature' in aValues && aValues['gui-show-temperature'] && 'temperature' in aValues) {
			$('#'+sTabId+'_'+sDevId+'_temp').text(aValues['temperature'].toFixed(aValues['gui-decimals']));
		}
		if('gui-show-sunriseset' in aValues && aValues['gui-show-sunriseset'] && 'sunrise' in aValues && 'sunset' in aValues) {
			$('#'+sTabId+'_'+sDevId+'_sunrise').text(aValues['sunrise'].toFixed(aValues['gui-decimals']));
			$('#'+sTabId+'_'+sDevId+'_sunset').text(aValues['sunset'].toFixed(aValues['gui-decimals']));
		}
		if('gui-show-update' in aValues && aValues['gui-show-update']) {
			iTime = Math.floor((new Date().getTime())/1000);

			if('timestamp' in aValues && 'min-interval' in aValues && aValues['timestamp'] > 0 && (iTime-aValues['timestamp']) > aValues['min-interval']) {
				if($('#'+sTabId+'_'+sDevId+'_weather_upd').attr('class').indexOf('update_active') == -1) {
					$('#'+sTabId+'_'+sDevId+'_weather_upd').removeClass('update_inactive').addClass('update_active');
				}
			} else {
				if($('#'+sTabId+'_'+sDevId+'_weather_upd').attr('class').indexOf('update_inactive') == -1) {
					$('#'+sTabId+'_'+sDevId+'_weather_upd').removeClass('update_active').addClass('update_inactive');
				}
			}
		}
	}
	oTab.listview();
	oTab.listview("refresh");
}

function updateProcStatus(aValues) {
	var obj = $('#proc').text("CPU: "+aValues['cpu'].toFixed(2)+"% / RAM: "+aValues['ram'].toFixed(2)+"%");
	obj.html(obj.html().replace(/\n/g,'<br/>'));
}

function createDateTimeElement(sTabId, sDevId, aValues) {
	aDateTime[sTabId+'_'+sDevId] = new Array();

	if('month' in aValues && aValues['month'] < 10) {
		aValues['month'] = '0'+aValues['month'];
	}
	if('day' in aValues && aValues['day'] < 10) {
		aValues['day'] = '0'+aValues['day'];
	}
	if('hour' in aValues && aValues['hour'] < 10) {
		aValues['hour'] = '0'+aValues['hour'];
	}
	if('minute' in aValues && aValues['minute'] < 10) {
		aValues['minute'] = '0'+aValues['minute'];
	}
	if('second' in aValues && aValues['second'] < 10) {
		aValues['second'] = '0'+aValues['second'];
	}

	aDateTime[sTabId+'_'+sDevId]['year'] = aValues['year'];
	aDateTime[sTabId+'_'+sDevId]['month'] = aValues['month'];
	aDateTime[sTabId+'_'+sDevId]['day'] = aValues['day'];
	aDateTime[sTabId+'_'+sDevId]['hour'] = aValues['hour'];
	aDateTime[sTabId+'_'+sDevId]['minute'] = aValues['minute'];
	aDateTime[sTabId+'_'+sDevId]['second'] = aValues['second'];

	if($('#'+sTabId+'_'+sDevId+'_datetime').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li id="'+sTabId+'_'+sDevId+'_datetime" data-icon="false"><div class="name">'+aValues['name']+'</div></li>'));
			oTab.find('#'+sTabId+'_'+sDevId+'_datetime').append($('<div id="'+sTabId+'_'+sDevId+'_text" class="datetime">'+aValues['year']+'-'+aValues['month']+'-'+aValues['day']+' '+aValues['hour']+':'+aValues['minute']+':'+aValues['second']+'</div>'));
		}
	} else {
		$('#'+sTabId+'_'+sDevId+'_text').text(aValues['year']+'-'+aValues['month']+'-'+aValues['day']+' '+aValues['hour']+':'+aValues['minute']+':'+aValues['second']);
	}
	oTab.listview();
	oTab.listview("refresh");
}

function createWebcamElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_webcam').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li class="webcam" id="'+sTabId+'_'+sDevId+'_webcam" data-icon="false" style="height: '+aValues['gui-image-height']+'px;"><div class="name">'+aValues['name']+'</div></li>'));
		}
		if('id' in aValues && 'url' in aValues['id'][0] && 'gui-image-height' in aValues) {
			oTab.find('#'+sTabId+'_'+sDevId+'_webcam').append($('<div class="webcam_image" id="'+sTabId+'_'+sDevId+'_image"><img id="'+sTabId+'_'+sDevId+'_img" src="'+aValues['id'][0]['url']+'" height="'+aValues['gui-image-height']+'"></div>'));
		}
		if('gui-image-width' in aValues && aValues['gui-image-width'] > 0) {
			$('#'+sTabId+'_'+sDevId+'_img').attr('width', aValues['gui-image-width']+'px');
		}
	}
	oTab.listview();
	oTab.listview("refresh");
	if('poll-interval' in aValues) {
		window.setInterval(function() {
			if('id' in aValues && 'url' in aValues['id'][0]) {
				if(aValues['id'][0]['url'].indexOf("?") != -1) {
					$('#'+sTabId+'_'+sDevId+'_img').attr('src', aValues['id'][0]['url']+"&"+new Date().getTime());
				} else {
					$('#'+sTabId+'_'+sDevId+'_img').attr('src', aValues['id'][0]['url']+"?"+new Date().getTime());
				}
			}
		}, aValues['poll-interval']*1000);
	}
}

function createXBMCElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_xbmc').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			oTab.append($('<li class="xbmc" id="'+sTabId+'_'+sDevId+'_xbmc" data-icon="false"><div class="name">'+aValues['name']+'</div></li>'));
		}
		if('gui-show-action' in aValues && aValues['gui-show-action']) {
			oTab.find('#'+sTabId+'_'+sDevId+'_xbmc').append($('<div class="action_icon" id="'+sTabId+'_'+sDevId+'_action">&nbsp;</div>'));
			if('action' in aValues) {
				if(aValues['action'] == "play") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('play');
				} else if(aValues['action'] == "pause") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('pause');
				} else if(aValues['action'] == "stop") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('stop');
				} else if(aValues['action'] == "active") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('screen_active');
				} else if(aValues['action'] == "inactive") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('screen_inactive');
				} else if(aValues['action'] == "shutdown") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('shutdown');
				} else if(aValues['action'] == "home") {
					$('#'+sTabId+'_'+sDevId+'_action').addClass('home');
				}
			}
		}
		if('gui-show-media' in aValues && aValues['gui-show-media']) {
			oTab.find('#'+sTabId+'_'+sDevId+'_xbmc').append($('<div class="media_icon" id="'+sTabId+'_'+sDevId+'_media">&nbsp;</div>'));
			if('media' in aValues) {
				if(aValues['media'] == "movie") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('movie');
				} else if(aValues['media'] == "episode") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('episode');
				} else if(aValues['media'] == "song") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('song');
				}
			}
		}
	} else {
		if('gui-show-action' in aValues && aValues['gui-show-action']) {
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("play") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("play");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("pause") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("pause");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("stop") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("stop");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("screen_active") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("screen_active");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("screen_inactive") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("screen_inactive");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("shutdown") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("shutdown");
			}
			if($('#'+sTabId+'_'+sDevId+'_action').attr("class").indexOf("home") != -1) {
				$('#'+sTabId+'_'+sDevId+'_action').removeClass("home");
			}
			if(aValues['action'] == "play") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('play');
			} else if(aValues['action'] == "pause") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('pause');
			} else if(aValues['action'] == "stop") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('stop');
			} else if(aValues['action'] == "active") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('screen_active');
			} else if(aValues['action'] == "inactive") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('screen_inactive');
			} else if(aValues['action'] == "shutdown") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('shutdown');
			} else if(aValues['action'] == "home") {
				$('#'+sTabId+'_'+sDevId+'_action').addClass('home');
			}
		}
		if('gui-show-media' in aValues && aValues['gui-show-media']) {
			if('media' in aValues) {
				if($('#'+sTabId+'_'+sDevId+'_media').attr("class").indexOf("movie") != -1) {
					$('#'+sTabId+'_'+sDevId+'_media').removeClass("movie");
				}
				if($('#'+sTabId+'_'+sDevId+'_media').attr("class").indexOf("episode") != -1) {
					$('#'+sTabId+'_'+sDevId+'_media').removeClass("episode");
				}
				if($('#'+sTabId+'_'+sDevId+'_media').attr("class").indexOf("music") != -1) {
					$('#'+sTabId+'_'+sDevId+'_media').removeClass("music");
				}
				if(aValues['media'] == "movie") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('movie');
				} else if(aValues['media'] == "episode") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('episode');
				} else if(aValues['media'] == "song") {
					$('#'+sTabId+'_'+sDevId+'_media').addClass('music');
				}
			}
		}
	}
	oTab.listview();
	oTab.listview("refresh");
}

function createDateTimeElement(sTabId, sDevId, aValues) {
	aDateTime[sTabId+'_'+sDevId] = new Array();

	if('month' in aValues && aValues['month'] < 10) {
		aValues['month'] = '0'+aValues['month'];
	}
	if('day' in aValues && aValues['day'] < 10) {
		aValues['day'] = '0'+aValues['day'];
	}
	if('hour' in aValues && aValues['hour'] < 10) {
		aValues['hour'] = '0'+aValues['hour'];
	}
	if('minute' in aValues && aValues['minute'] < 10) {
		aValues['minute'] = '0'+aValues['minute'];
	}
	if('second' in aValues && aValues['second'] < 10) {
		aValues['second'] = '0'+aValues['second'];
	}

	aDateTime[sTabId+'_'+sDevId]['year'] = aValues['year'];
	aDateTime[sTabId+'_'+sDevId]['month'] = aValues['month'];
	aDateTime[sTabId+'_'+sDevId]['day'] = aValues['day'];
	aDateTime[sTabId+'_'+sDevId]['hour'] = aValues['hour'];
	aDateTime[sTabId+'_'+sDevId]['minute'] = aValues['minute'];
	aDateTime[sTabId+'_'+sDevId]['second'] = aValues['second'];

	if('gui-datetime-format' in aValues) {
		sFormat = aValues['gui-datetime-format'];
	} else {
		sFormat = sDateTimeFormat;
	}
	aDateTimeFormats[sTabId+'_'+sDevId] = sFormat;

	if($('#'+sTabId+'_'+sDevId+'_datetime').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		if('name' in aValues) {
			sDate = moment(aValues['year']+'-'+aValues['month']+'-'+aValues['day']+' '+aValues['hour']+':'+aValues['minute']+':'+aValues['second'], ["YYYY-MM-DD HH:mm:ss"]).format(sFormat);
			oTab.append($('<li id="'+sTabId+'_'+sDevId+'_datetime" data-icon="false"><div class="name">'+aValues['name']+'</div></li>'));
			oTab.find('#'+sTabId+'_'+sDevId+'_datetime').append($('<div id="'+sTabId+'_'+sDevId+'_text" class="datetime">'+sDate+'</div>'));
		}
	} else {
		sDate = moment(aValues['year']+'-'+aValues['month']+'-'+aValues['day']+' '+aValues['hour']+':'+aValues['minute']+':'+aValues['second'], ["YYYY-MM-DD HH:mm:ss"]).format(sFormat);
		$('#'+sTabId+'_'+sDevId+'_text').text(sDate);
	}
	oTab.listview();
	oTab.listview("refresh");
}

function updateVersions() {
	if(iPLVersion < iPLNVersion) {
		if(iFWVersion > 0) {
			var obj = $('#version').text("pilight v"+iPLVersion+" - "+language.available+" v"+iPLNVersion+" / filter firmware v"+iFWVersion);
		} else {
			var obj = $('#version').text("pilight v"+iPLVersion+" - "+language.available+" v"+iPLNVersion);
		}
	} else {
		if(iFWVersion > 0) {
			var obj = $('#version').text("pilight v"+iPLVersion+" / filter firmware  v"+iFWVersion);
		} else {
			var obj = $('#version').text("pilight v"+iPLVersion);
		}
	}
	obj.html(obj.html().replace(/\n/g,'<br/>'));
}

function createGUI(data) {
	$.each(data, function(root, locations) {
		if(oWebsocket) {
			$('#proc').text("CPU: ...% / RAM: ...%");
		}
		if(root == 'version') {
			iPLVersion = locations[0];
			iPLNVersion = locations[1];
			updateVersions();
		} else if(root == 'firmware' && 'version' in locations) {
			iFWVersion = locations["version"];
			if(iFWVersion > 0) {
				updateVersions();
			}
		} else if(root == 'config') {
			$('#tabs').append($("<ul></ul>"));
			$.each(locations, function(lindex, lvalues) {
				$.each(lvalues, function(dindex, dvalues) {
					if(dindex == 'name') {
						if($('#'+lindex).length == 0) {
							if(bShowTabs) {
								var oNavBar = $('#tabs');
								var oLi = $("<li></li>");
								var oA  = $('<a href="#'+lindex+'"></a>');
								oA.text(dvalues);
								oLi.append(oA);
								oNavBar.find("*").andSelf().each(function(){
									$(this).removeClass(function(i, cn){
										var matches = cn.match(/ui-[\w\-]+/g) || [];
										return (matches.join (' '));
									});
									if($(this).attr("class") == "") {
										$(this).removeAttr("class");
									}
								});
								oNavBar.navbar("destroy");
								oLi.appendTo($("#tabs ul"));
								oNavBar.navbar();
								$('#content').append($('<div class="content" id="'+lindex+'"><ul data-role="listview" data-inset="true" data-theme="c"></ul></div>'));
							}
						}
						if(!bShowTabs) {
							if($('#heading_'+lindex).length == 0) {
								if($('#all').length == 0) {
									$('#content').append($('<ul id="all" data-role="listview" data-inset="true" data-theme="c"><li data-role="list-divider" id="heading_'+lindex+'" class="heading" role="heading">'+dvalues+'</li></ul>'));
								} else {
									$('#all').append($('<li data-role="list-divider" id="heading_'+lindex+'" class="heading" role="heading">'+dvalues+'</li>'));
								}
							}
						}
					} else if(dindex != 'order') {
						aValues = new Array();
						$.each(dvalues, function(sindex, svalues) {
							aValues[sindex] = svalues;
						});
						if(aValues['type'] == 1 || aValues['type'] == 4) {
							createSwitchElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 2) {
							createDimmerElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 3) {
							createWeatherElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 5) {
							createScreenElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 7) {
							createPendingSwitchElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 8) {
							createDateTimeElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 9) {
							createXBMCElement(lindex, dindex, aValues);
						} else if(aValues['type'] == 11) {
							createWebcamElement(lindex, dindex, aValues);
						}
					}
				});
			});

			if(bShowTabs) {
				$(document).delegate('[data-role="navbar"] a', 'click', function(e) {
					var iPos = this.href.indexOf('#');
					var iLen = this.href.length;
					var sId = this.href.substring(iPos, iLen);

					$('#content .content').each(function() {
						if(this.id != sId.substring(1, sId.length) && $(this).css("display") != 'none') {
							$(this).toggle();
						}
					});

					if($(sId).css("display") == "none") {
						$(sId).toggle();
					}
					e.preventDefault();
				});
				if(!bInitialized) {
					$('#tabs a').each(function() {
						$(this).click();
						$(this).addClass("ui-btn-active");
						return false;
					});
				}
			}
			$.mobile.loading("hide");
			bInitialized = true;
		}
	});
}

function parseData(data) {
	if(data.hasOwnProperty("config")) {
		createGUI(data);
	} else if(data.hasOwnProperty("values") && data.hasOwnProperty("origin") && data.hasOwnProperty("type")) {
		var aValues = data.values;
		var iType = data.type;

		if(iType == -1) {
			updateProcStatus(aValues);
		} else if(data.hasOwnProperty("devices")) {
			var aLocations = data.devices;
			$.each(aLocations, function(lindex, lvalues) {
				$.each(lvalues, function(dindex, dvalues) {
					$.each(aValues, function(vindex, vvalues) {
						if(iType == 1 || iType == 4) {
							if(vindex == 'state') {
								if(vvalues == 'on' || vvalues == 'opened') {
									$('#'+lindex+'_'+dvalues+'_switch')[0].selectedIndex = 1;
								} else {
									$('#'+lindex+'_'+dvalues+'_switch')[0].selectedIndex = 0;
								}
								$('#'+lindex+'_'+dvalues+'_switch').slider('refresh');
							}
						} else if(iType == 2) {
							if(vindex == 'state') {
								if(vvalues == 'on') {
									$('#'+lindex+'_'+dvalues+'_switch')[0].selectedIndex = 1;
								} else {
									$('#'+lindex+'_'+dvalues+'_switch')[0].selectedIndex = 0;
								}
								$('#'+lindex+'_'+dvalues+'_switch').slider('refresh');
							}
							if(vindex == 'dimlevel') {
								$('#'+lindex+'_'+dvalues+'_dimmer').val(vvalues);
								$('#'+lindex+'_'+dvalues+'_dimmer').slider('refresh');
							}
						} else if(iType == 8) {
							if(lindex+'_'+dvalues in aDateTime &&
							   vindex in aDateTime[lindex+'_'+dvalues]) {
								if(vvalues < 10) {
									vvalues = '0'+vvalues;
								}
								aDateTime[lindex+'_'+dvalues][vindex] = vvalues;
								aVal = aDateTime[lindex+'_'+dvalues];
								if('year' in aVal &&
								   'month' in aVal &&
								   'day' in aVal &&
								   'hour' in aVal &&
								   'minute' in aVal &&
								   'second' in aVal) {
									if(vindex == 'second') {
										sDate = moment(aVal['year']+'-'+aVal['month']+'-'+aVal['day']+' '+aVal['hour']+':'+aVal['minute']+':'+aVal['second'], ["YYYY-MM-DD HH:mm:ss"]).format(aDateTimeFormats[lindex+'_'+dvalues]);
										$('#'+lindex+'_'+dvalues+'_text').text(sDate);
									}
								}
							}
						} else if(iType == 7) {
							if(vindex == 'state') {
								if($('#'+lindex+'_'+dvalues+'_pendingsw').parent().attr("class").indexOf("ui-icon-loader") >= 0) {
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().removeClass('ui-icon-loader');
								}
								if($('#'+lindex+'_'+dvalues+'_pendingsw').parent().attr("class").indexOf("ui-icon-on") >= 0) {
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().removeClass('ui-icon-on');
								}
								if($('#'+lindex+'_'+dvalues+'_pendingsw').parent().attr("class").indexOf("ui-icon-off") >= 0) {
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().removeClass('ui-icon-off');
								}
								if(vvalues == 'running') {
									if(lindex+'_'+dvalues in aReadOnly && aReadOnly[lindex+'_'+dvalues] == 0) {
										$('#'+lindex+'_'+dvalues+'_pendingsw').button('enable');
									}
									$('#'+lindex+'_'+dvalues+'_pendingsw').text(language.running);
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().addClass('ui-icon-on');
								} else if(vvalues == 'pending') {
									$('#'+lindex+'_'+dvalues+'_pendingsw').button('disable');
									$('#'+lindex+'_'+dvalues+'_pendingsw').text(language.toggling);
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().addClass('ui-icon-loader')
								} else {
									$('#'+lindex+'_'+dvalues+'_pendingsw').button('enable');
									$('#'+lindex+'_'+dvalues+'_pendingsw').text(language.stopped);
									$('#'+lindex+'_'+dvalues+'_pendingsw').parent().addClass('ui-icon-off')
								}
								$('#'+lindex+'_'+dvalues+'_pendingsw').button('refresh');
							}
						} else if(iType == 3) {
							if(vindex == 'temperature' && $('#'+lindex+'_'+dvalues+'_temp')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]['device']);
									$('#'+lindex+'_'+dvalues+'_temp').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'humidity' && $('#'+lindex+'_'+dvalues+'_humi')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]['device']);
									$('#'+lindex+'_'+dvalues+'_humi').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'rain' && $('#'+lindex+'_'+dvalues+'_rain')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]['device']);
									$('#'+lindex+'_'+dvalues+'_rain').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'windgust' && $('#'+lindex+'_'+dvalues+'_windgust')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									$('#'+lindex+'_'+dvalues+'_windgust').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'winddir' && $('#'+lindex+'_'+dvalues+'_winddir')) {
								$('#'+lindex+'_'+dvalues+'_weather .winddir_icon').css({transform: 'rotate(' + vvalues + 'deg)'});
							} else if(vindex == 'windavg' && $('#'+lindex+'_'+dvalues+'_windavg')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									$('#'+lindex+'_'+dvalues+'_windavg').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'sunrise' && $('#'+lindex+'_'+dvalues+'_sunrise')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]['device']);
									$('#'+lindex+'_'+dvalues+'_sunrise').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'sunset' && $('#'+lindex+'_'+dvalues+'_sunset')) {
								if(lindex+'_'+dvalues in aDecimals
								&& 'device' in aDecimals[lindex+'_'+dvalues]
								&& 'gui' in aDecimals[lindex+'_'+dvalues]) {
									vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]['device']);
									$('#'+lindex+'_'+dvalues+'_sunset').text(vvalues.toFixed(aDecimals[lindex+'_'+dvalues]['gui']));
								}
							} else if(vindex == 'battery' && $('#'+lindex+'_'+dvalues+'_batt')) {
								if(vvalues == 1) {
									if($('#'+lindex+'_'+dvalues+'_batt').attr("class").indexOf("green") == -1) {
										$('#'+lindex+'_'+dvalues+'_batt').removeClass('red').addClass('green');
									}
								} else {
									if($('#'+lindex+'_'+dvalues+'_batt').attr("class").indexOf("red") == -1) {
										$('#'+lindex+'_'+dvalues+'_batt').removeClass('green').addClass('red');
									}
								}
							} else if(vindex == 'sun' && $('#'+lindex+'_'+dvalues+'_sunrise_icon') && $('#'+lindex+'_'+dvalues+'_sunset_icon')) {
								if(vvalues == 'rise') {
									if($('#'+lindex+'_'+dvalues+'_sunrise_icon').attr("class").indexOf("yellow") == -1) {
										$('#'+lindex+'_'+dvalues+'_sunrise_icon').removeClass('gray').addClass('yellow');
									}
									if($('#'+lindex+'_'+dvalues+'_sunset_icon').attr("class").indexOf("gray") == -1) {
										$('#'+lindex+'_'+dvalues+'_sunset_icon').removeClass('blue').addClass('gray');
									}
								} else {
									if($('#'+lindex+'_'+dvalues+'_sunrise_icon').attr("class").indexOf("gray") == -1) {
										$('#'+lindex+'_'+dvalues+'_sunrise_icon').removeClass('yellow').addClass('gray');
									}
									if($('#'+lindex+'_'+dvalues+'_sunset_icon').attr("class").indexOf("blue") == -1) {
										$('#'+lindex+'_'+dvalues+'_sunset_icon').removeClass('gray').addClass('blue');
									}
								}
							}
							if(vindex == 'timestamp' && aTimers[lindex+'_'+dvalues] && aMinInterval[lindex+'_'+dvalues] && aPollInterval[lindex+'_'+dvalues]) {
								iTime = Math.floor((new Date().getTime())/1000);
								iTimeOut = (aMinInterval[lindex+'_'+dvalues]-((vvalues-aValues['timestamp'])))*1000;
								if($('#'+lindex+'_'+dvalues+'_upd').attr('class').indexOf('update_inactive') == -1) {
									$('#'+lindex+'_'+dvalues+'_upd').removeClass('update_active').addClass('update_inactive');
								}
								window.clearTimeout(aTimers[lindex+'_'+dvalues]);
								aTimers[lindex+'_'+dvalues] = window.setTimeout(function() {
									if($('#'+lindex+'_'+dvalues+'_upd').attr("class").indexOf('update_inactive') != -1) {
										$('#'+lindex+'_'+dvalues+'_upd').removeClass('update_inactive').addClass('update_active');
									}
								}, iTimeOut);
							}
						} else if(iType == 9) {
							if(vindex == "action" && $('#'+lindex+'_'+dvalues+'_action')) {
								if($('#'+lindex+'_'+dvalues+'_action')) {
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("play") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("play");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("pause") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("pause");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("stop") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("stop");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("screen_active") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("screen_active");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("screen_inactive") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("screen_inactive");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("shutdown") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("shutdown");
									}
									if($('#'+lindex+'_'+dvalues+'_action').attr("class").indexOf("home") != -1) {
										$('#'+lindex+'_'+dvalues+'_action').removeClass("home");
									}
								}
								if(vvalues == "play") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("play");
								} else if(vvalues == "pause") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("pause");
								} else if(vvalues == "stop") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("stop");
								} else if(vvalues == "active") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("screen_active");
								} else if(vvalues == "inactive") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("screen_inactive");
								} else if(vvalues == "shutdown") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("shutdown");
								} else if(vvalues == "home") {
									$('#'+lindex+'_'+dvalues+'_action').addClass("home");
								}
							}
							if(vindex == "media" && $('#'+lindex+'_'+dvalues+'_media')) {
								if($('#'+lindex+'_'+dvalues+'_media')) {
									if($('#'+lindex+'_'+dvalues+'_media').attr("class").indexOf("movie") != -1) {
										$('#'+lindex+'_'+dvalues+'_media').removeClass("movie");
									}
									if($('#'+lindex+'_'+dvalues+'_media').attr("class").indexOf("episode") != -1) {
										$('#'+lindex+'_'+dvalues+'_media').removeClass("episode");
									}
									if($('#'+lindex+'_'+dvalues+'_media').attr("class").indexOf("song") != -1) {
										$('#'+lindex+'_'+dvalues+'_media').removeClass("song");
									}
								}
								if(vvalues == "movie") {
									$('#'+lindex+'_'+dvalues+'_media').addClass("movie");
								} else if(vvalues == "episode") {
									$('#'+lindex+'_'+dvalues+'_media').addClass("episode");
								} else if(vvalues == "song") {
									$('#'+lindex+'_'+dvalues+'_media').addClass("song");
								}
							}
						}
					});
				});
			});
		}
	}
}

$(document).ready(function() {
	if($('body').length == 1) {
		$.mobile.loading('show', {
			'text': language.connecting,
			'textVisible': true,
			'theme': 'b'
		});

		if((navigator.userAgent.match(/iPhone/i)) ||
		   (navigator.userAgent.match(/iPod/i)) ||
		   (navigator.userAgent.match(/iPad/i))) {
			bForceAjax = true;
		}
		if(!bForceAjax && typeof MozWebSocket != "undefined") {
			oWebsocket = new MozWebSocket("ws://"+location.host);
		} else if(!bForceAjax && typeof WebSocket != "undefined") {
			/* The characters after the trailing slash are needed for a wierd IE 10 bug */
			oWebsocket = new WebSocket("ws://"+location.host+'/websocket');
		} else {
			var load = window.setInterval(function() {
				$.get('http://'+location.host+'/config?'+$.now(), function(txt) {
					bConnected = true;
					if(!bSending) {
						var data = $.parseJSON(txt);
						parseData(data);
					}
				}).fail(function() {
					window.clearInterval(load);
					if(bConnected) {
						$.mobile.loading('show', {
							'text': language.connection_lost,
							'textVisible': true,
							'theme': 'b'
						});
						$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
					} else {
						$.mobile.loading('show', {
							'text': language.connection_failed,
							'textVisible': true,
							'theme': 'b'
						});
						$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
					}
				});
			}, 1000);
		}

		if(!bForceAjax && oWebsocket) {
			oWebsocket.onopen = function(evt) {
				bConnected = true;
				oWebsocket.send("{\"message\":\"request config\"}");
			};
			oWebsocket.onclose = function(evt) {
				if(bConnected) {
					$.mobile.loading('show', {
						'text': language.connection_lost,
						'textVisible': true,
						'theme': 'b'
					});
					$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
				} else {
					$.mobile.loading('show', {
						'text': language.connection_failed,
						'textVisible': true,
						'theme': 'b'
					});
					$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
				}
			};
			oWebsocket.onerror = function(evt) {
				$.mobile.loading('show', {
					'text': language.unexpected_error,
					'textVisible': true,
					'theme': 'b'
				});
			};
			oWebsocket.onmessage = function(evt) {
				var data = $.parseJSON(evt.data);
				parseData(data);
			}
		}
		$('div[data-role="header"] h1').css({'white-space':'normal'});
	}
});
