var websocket;
var bConnected = false;

function createSwitchElement(sTabId, sDevId, sDevName, sDevProto, sState) {
	oTab = $('#'+sTabId).find('ul');
	oTab.append($('<li data-icon="false">'+sDevName+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select></li>'));
	$('#'+sTabId+'_'+sDevId+'_switch').slider();
	$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
		event.stopPropagation();
		websocket.send('{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}');
	});
	oTab.listview();
	oTab.listview("refresh");
	if(sState == "on") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
}

function createDimmerElement(sTabId, sDevId, sDevName, sDevProto, sState, iDimLevel) {
	iOldDimLevel = iDimLevel;
	oTab = $('#'+sTabId).find('ul');
	if(sDevProto == "kaku_dimmer") {
		iMin = 0;
		iMax = 15;
	}
	oTab.append($('<li data-icon="false">'+sDevName+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select><input type="range" name="slider-fill" id="'+sTabId+'_'+sDevId+'_dimmer" class="dimmer-slider" value="'+iDimLevel+'" min="'+iMin+'" max="'+iMax+'" data-highlight="true" /></li>'));
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
	if(sState == "on") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
}

function createWeatherElement(sTabId, sDevId, sDevName, sDevProto, iTemperature, iPrecisionTemperature, iHumidity, iPrecisionHumidity, iBattery) {
	oTab = $('#'+sTabId).find('ul');
        if(sDevProto == "alecto") {
	    iPrecisionTemperature = 10;
	    iPrecisionHumidity = 1;
	}
        else {
	    iPrecisionTemperature = Math.pow(10, iPrecisionTemperature);
	    iPrecisionHumidity = Math.pow(10, iPrecisionHumidity); 
	}
        iTemperature /= iPrecisionTemperature;
        iHumidity /= iPrecisionHumidity;
    
    	oTab.append($('<li data-icon="false">'+sDevName+'<div class="temperature" id="'+sTabId+'_'+sDevId+'_temp">'+(iTemperature)+'</div><div class="degrees">o</div><div class="humidity" id="'+sTabId+'_'+sDevId+'_humi">'+iHumidity+'</div><div class="percentage">%</div></li>'));
    oTab.find('li').append($('<div id="'+sTabId+'_'+sDevId+'_batt" class="battery"></div>'));
    if(iBattery) {
		$('#'+sTabId+'_'+sDevId+'_batt').addClass('green');
	} else {
		$('#'+sTabId+'_'+sDevId+'_batt').addClass('red');
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
						var sDevName;
						var iDevType;
						var sDevState;
						var iDevDimLevel;
						var sDevProto;
						var iHumidity;
						var iBattery;
						var iTemperature;
					    	var iPrecisionTemperature;
					    	var iPrecisionHumidity;
					    
						$.each(dvalues, function(sindex, svalues) {
							if(sindex == 'name') {
								sDevName = svalues;
							} else if(sindex == 'type') {
								iDevType = svalues;
							} else if(sindex == 'state') {
								sDevState = svalues;
							} else if(sindex == 'dimlevel') {
								iDimLevel = svalues;
							} else if(sindex == 'protocol') {
								sDevProto = svalues;
							} else if(sindex == 'humidity') {
								iHumidity = svalues;
							} else if(sindex == 'battery') {
								iBattery = svalues;
							} else if(sindex == 'temperature') {
								iTemperature = svalues;
							} else if(sindex == 'precision_temperature') {
								iPrecisionTemperature = svalues;
							} else if(sindex == 'precision_humidity') {
								iPrecisionHumidity = svalues;
							}
						    
						});
						if(iDevType == 1) {
							createSwitchElement(lindex, dindex, sDevName, sDevProto, sDevState);
						} else if(iDevType == 2) {
							createDimmerElement(lindex, dindex, sDevName, sDevProto, sDevState, iDimLevel);
						} else if(iDevType == 3) {
							createWeatherElement(lindex, dindex, sDevName, sDevProto, iTemperature, iPrecisionTemperature, iHumidity, iPrecisionHumidity, iBattery);
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
				$.each(aValues, function(vindex, vvalues) {
					if(iType == 1) {
						if(vindex == 'state') {
							if(vvalues == 'on') {
								$('#'+lindex+'_'+lvalues+'_switch')[0].selectedIndex = 1;
							} else {
								$('#'+lindex+'_'+lvalues+'_switch')[0].selectedIndex = 0;
							}
							$('#'+lindex+'_'+lvalues+'_switch').slider('refresh');
						}
					} else if(iType == 2) {
						if(vindex == 'state') {
							if(vvalues == 'on') {
								$('#'+lindex+'_'+lvalues+'_switch')[0].selectedIndex = 1;
							} else {
								$('#'+lindex+'_'+lvalues+'_switch')[0].selectedIndex = 0;
							}
							$('#'+lindex+'_'+lvalues+'_switch').slider('refresh');
						}
						if(vindex == 'dimlevel') {
							$('#'+lindex+'_'+lvalues+'_dimmer').val(vvalues);
							$('#'+lindex+'_'+lvalues+'_dimmer').slider('refresh');
						}
					} else if(iType == 3) {
						if(vindex == 'temperature') {
						    if (aValues.hasOwnProperty("precision_temperature")) {
							vvalues /= Math.pow(10, aValues.precision_temperature);
						    } else {
							vvalues /= 10; //assume alecto??
						    }
						    $('#'+lindex+'_'+lvalues+'_temp').text(vvalues);
						} else if(vindex == 'humidity') {
						    if (aValues.hasOwnProperty("precision_humidity")) {
							vvalues /= Math.pow(10, aValues.precision_humidity);
  						    } else {
							vvalues /= 1; //assume alecto??
						    }
						    $('#'+lindex+'_'+lvalues+'_temp').text(vvalues);

						    if(vvalues > 1000) {
							vvalues /= 100;
						    }	
						    else if(vvalues > 100) {
							vvalues /= 10;
						    }

						    $('#'+lindex+'_'+lvalues+'_humi').text(vvalues);
						} else if(vindex == 'battery') {
							if(vvalues == 1) {
								$('#'+lindex+'_'+lvalues+'_batt').removeClass('red').addClass('green');
							} else {
								$('#'+lindex+'_'+lvalues+'_batt').removeClass('green').addClass('red');
							}
						}
					}
				});
			});
		};	
	}
});
