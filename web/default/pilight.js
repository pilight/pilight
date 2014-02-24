var oWebsocket = false;
var bConnected = false;
var bInitialized = false;
var bSending = false;
var aDecimals = new Array();
var aPrice = new Array();
var bShowTabs = true;
var iPLVersion = 0;
var iPLNVersion = 0;
var iFWVersion = 0;

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
		oTab.append($('<li id="'+sTabId+'_'+sDevId+'" class="switch" data-icon="false">'+aValues['name']+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select></li>'));
		$('#'+sTabId+'_'+sDevId+'_switch').slider();
		$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
			event.stopPropagation();
			var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
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
	if(aValues['state'] === "on" || aValues['state'] === "opened") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	} else {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 0;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
	if(aValues['settings']['readonly']) {
		$('#'+sTabId+'_'+sDevId+'_switch').slider('disable');
	}
}

function createScreenElement(sTabId, sDevId, aValues) {
	if($('#'+sTabId+'_'+sDevId+'_screen').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		oTab.append($('<li  id="'+sTabId+'_'+sDevId+'" class="screen" data-icon="false">'+aValues['name']+'<div id="'+sTabId+'_'+sDevId+'_screen" class="screen" data-role="fieldcontain" data-type="horizontal"><fieldset data-role="controlgroup" class="controlgroup" data-type="horizontal" data-mini="true"><input type="radio" name="'+sTabId+'_'+sDevId+'_screen" id="'+sTabId+'_'+sDevId+'_screen_down" value="down" /><label for="'+sTabId+'_'+sDevId+'_screen_down">Down</label><input type="radio" name="'+sTabId+'_'+sDevId+'_screen" id="'+sTabId+'_'+sDevId+'_screen_up" value="up" /><label for="'+sTabId+'_'+sDevId+'_screen_up">Up</label></fieldset></div></li>'));
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
				if(i==4)
					window.clearInterval(x);
			}, 150);
			var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}'
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
				if(i==4)
					window.clearInterval(x);
			}, 150);
			var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
			if(oWebsocket) {
				oWebsocket.send(json);
			} else {
				bSending = true;
				$.get('http://'+location.host+'/send?'+encodeURIComponent(json));
				window.setTimeout(function() { bSending = false; }, 1000);
			}
		});
	}
	if(aValues['state'] == "up") {
		$('#'+sTabId+'_'+sDevId+'_screen_up').attr("checked","checked")
		$('#'+sTabId+'_'+sDevId+'_screen_up').checkboxradio("refresh");
	} else {
		$('#'+sTabId+'_'+sDevId+'_screen_down').attr("checked","checked")
		$('#'+sTabId+'_'+sDevId+'_screen_down').checkboxradio("refresh");
	}
	oTab.listview();
	oTab.listview("refresh");
	if(aValues['settings']['readonly']) {
		$('#'+sTabId+'_'+sDevId+'_screen_up').checkboxradio('disable');
		$('#'+sTabId+'_'+sDevId+'_screen_down').checkboxradio('disable');
	}
}

function createDimmerElement(sTabId, sDevId, aValues) {
	iOldDimLevel = aValues['dimlevel'];
	if($('#'+sTabId+'_'+sDevId+'_switch').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		oTab.append($('<li id="'+sTabId+'_'+sDevId+'" class="dimmer" data-icon="false">'+aValues['name']+'<select id="'+sTabId+'_'+sDevId+'_switch" data-role="slider"><option value="off">Off</option><option value="on">On</option></select><div id="'+sTabId+'_'+sDevId+'_dimmer" min="'+aValues['settings']['min']+'" max="'+aValues['settings']['max']+'" data-highlight="true" ><input type="value" id="'+sTabId+'_'+sDevId+'_value" class="slider-value dimmer-slider ui-slider-input ui-input-text ui-body-c ui-corner-all ui-shadow-inset" /></div></li>'));
		$('#'+sTabId+'_'+sDevId+'_switch').slider();
		$('#'+sTabId+'_'+sDevId+'_switch').bind("change", function(event, ui) {
			event.stopPropagation();
			var json = '{"message":"send","code":{"location":"'+sTabId+'","device":"'+sDevId+'","state":"'+this.value+'"}}';
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
	if(aValues['state'] == "on") {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 1;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	} else {
		$('#'+sTabId+'_'+sDevId+'_switch')[0].selectedIndex = 0;
		$('#'+sTabId+'_'+sDevId+'_switch').slider('refresh');
	}
	if(aValues['settings']['readonly']) {
		$('#'+sTabId+'_'+sDevId+'_switch').slider('disable');
		$('#'+sTabId+'_'+sDevId+'_dimmer').slider('disable');
	}
}

function createWeatherElement(sTabId, sDevId, aValues) {
	aDecimals[sTabId+'_'+sDevId] = aValues['settings']['decimals'];
	aValues['temperature'] /= Math.pow(10, aValues['settings']['decimals']).toFixed(aValues['settings']['decimals']);
	aValues['humidity'] /= Math.pow(10, aValues['settings']['decimals']).toFixed(aValues['settings']['decimals']);
	if($('#'+sTabId+'_'+sDevId+'_weather').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
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
	} else {
		if(aValues['settings']['battery']) {
			if(aValues['battery']) {
				$('#'+sTabId+'_'+sDevId+'_batt').removeClass('red').addClass('green');
			} else {
				$('#'+sTabId+'_'+sDevId+'_batt').removeClass('green').addClass('red');
			}
		}
		if(aValues['settings']['humidity']) {
			$('#'+sTabId+'_'+sDevId+'_humi').text(aValues['humidity']);
		}
		if(aValues['settings']['temperature']) {
			$('#'+sTabId+'_'+sDevId+'_temp').text(aValues['temperature']);
		}
	}
	oTab.listview();
	oTab.listview("refresh");
}

function PlayImg(Wurl,imgid,Winterval,Wdivid,Wmaxwidth)
{
	console.log(imgid+' '+Winterval);
	var imgsrc = '';
	if (Wurl.indexOf("?") != -1) {
		imgsrc = Wurl+"&timestamp="+new Date().getTime();
        } else {
            	imgsrc = Wurl+"?"+new Date().getTime();
        }
	var img = document.createElement("img");
	img.setAttribute("id", imgid);
	console.log('imagen creada '+imgid);
	if (Wmaxwidth > 0) {
		img.width = Wmaxwidth;
	}
	console.log('tamaño asignado '+imgid);
	img.onload = function () {
		console.log('Cargamos imagen '+imgid);
		var div = document.getElementById(Wdivid);
		while (div.childNodes.length > 0)
			div.removeChild(div.childNodes[0]);
		div.appendChild(img);			
		window.setTimeout(function(){PlayImg(Wurl,imgid,Winterval,Wdivid,Wmaxwidth)}, Winterval);
	}
	img.onerror = function () {
		console.log('Error en carga imagen '+imgid);
		window.setTimeout(function(){PlayImg(Wurl,imgid,Winterval,Wdivid,Wmaxwidth)}, Winterval);
	}
	console.log(imgsrc);
	img.src = imgsrc;
}

function createWebcamElement(sTabId, sDevId, aValues) {
	var Wname = aValues['name'];
	var Wurl = aValues['id'][0]['url'];
	var Winterval = aValues['settings']['interval'];
	var Wmaxwidth = aValues['settings']['width'];
	var Wdivid = sTabId+'_'+sDevId+'_image';
	var imgid = sTabId+"_"+sDevId+"_img";
	if($('#'+sTabId+'_'+sDevId+'_webcam').length == 0) {
		if(bShowTabs) {
			oTab = $('#'+sTabId).find('ul');
		} else {
			oTab = $('#all');
		}
		oTab.append($('<li class="webcam" id="'+sTabId+'_'+sDevId+'_webcam" data-icon="false">'+Wname+'</li>'));
		if(Wurl) {
			if (Wmaxwidth > 0) {
				oTab.find('#'+sTabId+'_'+sDevId+'_webcam').append($('<div class="webcam" id="'+sTabId+'_'+sDevId+'_image"><img id="'+sTabId+'_'+sDevId+'_img" class="imgrespon" src="'+Wurl+'" width='+Wmaxwidth+'></div>'));
			} else {
				oTab.find('#'+sTabId+'_'+sDevId+'_webcam').append($('<div class="webcam" id="'+sTabId+'_'+sDevId+'_image"><img id="'+sTabId+'_'+sDevId+'_img" class="imgrespon" src="'+Wurl+'"></div>'));
			}
		}

	} else {
		if(Wurl) {
			$('#'+sTabId+'_'+sDevId+'_image').text(Wurl);
		}
	}
	oTab.listview();
	oTab.listview("refresh");
        var aWebcam = window.setTimeout(function(){PlayImg(Wurl,imgid,Winterval,Wdivid,Wmaxwidth)},Winterval);
}

function updateVersions() {
	if(iPLVersion != iPLNVersion) {
		if(iFWVersion > 0) {
			var obj = $('#version').text("pilight v"+iPLVersion+" - available v"+iPLNVersion+" / filter firmware v"+iFWVersion);
		} else {
			var obj = $('#version').text("pilight v"+iPLVersion+" - available v"+iPLNVersion);
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
		if(root == 'version') {
			iPLVersion = locations[0];
			iPLNVersion = locations[1];
			updateVersions();
		} else if(root == "firmware") {
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
						} else if(aValues['type'] == 6) {
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
			$.mobile.hidePageLoadingMsg();
			bInitialized = true;
		}
	});
}

function parseData(data) {
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
					} else if(iType == 3) {
						if(vindex == 'temperature' && $('#'+lindex+'_'+dvalues+'_temp')) {
							vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]).toFixed(aDecimals[lindex+'_'+dvalues]);
							$('#'+lindex+'_'+dvalues+'_temp').text(vvalues);
						} else if(vindex == 'humidity' && $('#'+lindex+'_'+dvalues+'_humi')) {
							vvalues /= Math.pow(10, aDecimals[lindex+'_'+dvalues]).toFixed(aDecimals[lindex+'_'+dvalues]);
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
	}
}

$(document).ready(function() {
	if($('body').length == 1) {
		$.mobile.showPageLoadingMsg("b", "Connecting...", true);
		if(typeof MozWebSocket != "undefined") {
			oWebsocket = new MozWebSocket("ws://"+location.host, "data");
		} else if(typeof WebSocket != "undefined") {
			/* The characters after the trailing slash are needed for a wierd IE 10 bug */
			oWebsocket = new WebSocket("ws://"+location.host+'/websocket', "data");
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
						$.mobile.showPageLoadingMsg("b", "Connection lost, touch to reload", true);
						$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
					} else {
						$.mobile.showPageLoadingMsg("b", "Failed to connect, touch to reload", true);
						$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
					}
				});
			}, 1000);
		}

		if(oWebsocket) {
			oWebsocket.onopen = function(evt) {
				bConnected = true;
				oWebsocket.send("{\"message\":\"request config\"}");
			};
			oWebsocket.onclose = function(evt) {
				if(bConnected) {
					$.mobile.showPageLoadingMsg("b", "Connection lost, touch to reload", true);
					$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
				} else {
					$.mobile.showPageLoadingMsg("b", "Failed to connect, touch to reload", true);
					$('html').on({ 'touchstart mousedown' : function(){location.reload();}});
				}
			};
			oWebsocket.onerror = function(evt) {
				$.mobile.showPageLoadingMsg("b", "An unexpected error occured", true);
			};
			oWebsocket.onmessage = function(evt) {
				var data = $.parseJSON(evt.data);
				parseData(data);
			}
		}
		$('div[data-role="header"] h1').css({'white-space':'normal'});
	}
});
