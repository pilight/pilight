var websocket;
var bConnected = false;

function createSwitchElement(sTabId, sDevId, sDevName, sState) {
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
							}
						});
						if(iDevType == 1) {
							createSwitchElement(lindex, dindex, sDevName, sDevState);
						} else if(iDevType == 2) {
							createDimmerElement(lindex, dindex, sDevName, sDevProto, sDevState, iDimLevel);
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
					}
				});
			});
		};	
	}
});