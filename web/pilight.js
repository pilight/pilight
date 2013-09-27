var websocket;
var bConnected = false;

var aDecimals = new Array();

function createSwitchElement(sTabId, sDevId, aValues) {
	oTab = $('#'+sTabId).find('ul');
	oTab.append($('<li data-icon="false">'+aValues['name']+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select></li>'));
	$('#'+sTabId+'_'+sDevId+'_switch').slider();
	$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
		event.stopPropagation();
		websocket.send('{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}');
	});
	oTab.listview();
	oTab.listview("refresh");
	if(aValues['state'] == "on") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
}

function createDimmerElement(sTabId, sDevId, aValues) {
	iOldDimLevel = aValues['dimlevel'];
	oTab = $('#'+sTabId).find('ul');
	oTab.append($('<li data-icon="false">'+aValues['name']+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select><input type="range" name="slider-fill" id="'+sTabId+'_'+sDevId+'_dimmer" class="dimmer-slider" value="'+aValues['dimlevel']+'" min="'+aValues['settings']['min']+'" max="'+aValues['settings']['max']+'" data-highlight="true" /></li>'));
	$('#'+sTabId+'_'+sDevId+'_switch').slider();
	$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
		event.stopPropagation();
		websocket.send('{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}');
	});

	$('#'+sTabId+'_'+sDevId+'_dimmer').slider({
		stop: function() {
			if(iOldDimLevel != this.value) {
				iOldDimLevel = this.value;
				$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
				$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
				websocket.send('{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"on","values":{"dimlevel":"'+this.value+'"}}}');
			}
		}    
	});
	
	oTab.listview();
	oTab.listview("refresh");
	if(aValues['state'] == "on") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
}

function createWeatherElement(sTabId, sDevId, aValues) {
	oTab = $('#'+sTabId).find('ul');
	aDecimals[sTabId+'_'+sDevId] = aValues['settings']['decimals'];
	aValues['temperature'] /= Math.pow(10, aValues['settings']['decimals']);
	aValues['humidity'] /= Math.pow(10, aValues['settings']['decimals']);
	oTab.append($('<li class="weather" id="'+sTabId+'_'+sDevId+'_weather" data-icon="false">'+aValues['name']+'</li>'));
	if(aValues['settings']['battery']) {
		oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div id="'+sTabId+'_'+sDevId+'_batt" class="battery"></div>'));
		if(aValues['battery']) {
			$('#'+sTabId+'_'+sDevId+'_batt').addClass('green');
		} else {
			$('#'+sTabId+'_'+sDevId+'_batt').addClass('red');
		}
	}	
	if(aValues['settings']['humidity']) {
		oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="percentage">%</div><div class="humidity" id="'+sTabId+'_'+sDevId+'_humi">'+aValues['humidity']+'</div>'));
	}
	if(aValues['settings']['temperature']) {
		oTab.find('#'+sTabId+'_'+sDevId+'_weather').append($('<div class="degrees">o</div><div class="temperature" id="'+sTabId+'_'+sDevId+'_temp">'+aValues['temperature']+'</div>'));
	}
	oTab.listview();
	oTab.listview("refresh");
}

function createGUI(data) {
	$.each(data, function(root, locations) {
		$('#tabs').append($("<ul></ul>"));
		if(root == 'config') {
			$.each(locations, function(lindex, lvalues) {
				$.each(lvalues, function(dindex, dvalues) {
					if(dindex == 'name') {
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
							if ($(this).attr("class") == "") {
								$(this).removeAttr("class");
							}
						});
						oNavBar.navbar("destroy");
						oLi.appendTo($("#tabs ul"));
						oNavBar.navbar();
						$('#content').append($('<div class="content" id="'+lindex+'"><ul data-role="listview" data-inset="true" data-theme="c"></ul></div>'));
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
						}
					}
				});
			});
		}
	});	
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
	$('#tabs a').each(function() {
		$(this).click();
		$(this).addClass("ui-btn-active");
		return false;
	});
	$.mobile.hidePageLoadingMsg();
}

$(document).ready(function() {
	if(typeof MozWebSocket != "undefined") {
		websocket = new MozWebSocket("ws://"+location.host);
	} else if(typeof WebSocket != "undefined") {
		websocket = new WebSocket("ws://"+location.host);
	} else {
		$.mobile.showPageLoadingMsg("b", "You're browser doesn't support websockets", true);
		return false;
	}

	$.mobile.showPageLoadingMsg("b", "Connecting...", true);
	
	websocket.onopen = function(evt) {
		bConnected = true;
		websocket.send("{\"message\":\"request config\"}");
	};
	websocket.onclose = function(evt) {
		if(bConnected)
			$.mobile.showPageLoadingMsg("b", "Connection lost", true);
		else
			$.mobile.showPageLoadingMsg("b", "Failed to connect", true);
	};
	websocket.onerror = function(evt) {
		$.mobile.showPageLoadingMsg("b", "An unexpected error occured", true);
	};

	websocket.onmessage = function(evt) {
		var data = $.parseJSON(evt.data);
		if(data.hasOwnProperty("config")) {
			createGUI(data);
		} else if(data.hasOwnProperty("values") && data.hasOwnProperty("origin") && data.hasOwnProperty("devices") && data.hasOwnProperty("type")) {
			var aValues = data.values;
			var aLocations = data.devices;
			var iType = data.type;
			$.each(aLocations, function(lindex, lvalues) {
				$.each(lvalues, function(dindex, dvalues) {
					$.each(aValues, function(vindex, vvalues) {
						if(iType == 1 || iType == 4) {
							if(vindex == 'state') {
								if(vvalues == 'on') {
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
						} else if(iType == 3) {
							if(vindex == 'temperature' && $('#'+lindex+'_'+dvalues+'_temp')) {
								vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]);
								$('#'+lindex+'_'+dvalues+'_temp').text(vvalues);
							} else if(vindex == 'humidity' && $('#'+lindex+'_'+dvalues+'_humi')) {
								vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]);
								$('#'+lindex+'_'+dvalues+'_humi').text(vvalues);
							} else if(vindex == 'battery' && $('#'+lindex+'_'+dvalues+'_batt')) {
								if(vvalues == 1) {
									$('#'+lindex+'_'+dvalues+'_batt').removeClass('red').addClass('green');
								} else {
									$('#'+lindex+'_'+dvalues+'_batt').removeClass('green').addClass('red');
								}
							}
						}
					});
				});
			});
		};	
	}
});