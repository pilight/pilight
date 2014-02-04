/*
Responsive Img jQuery Plugin
Version 1.5
Nov 23rd, 2013

Documentation: http://responsiveimg.com
Repository: https://github.com/drewbrolik/Responsive-Img

Copyright 2012 - 2013 Drew Thomas and Brolik

Dual licensed under the MIT and GPL licenses:
https://github.com/jquery/jquery/blob/master/MIT-LICENSE.txt
http://www.gnu.org/licenses/gpl.txt

This file is part of Responsive Img.

Responsive Img is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Responsive Img is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Responsive Img.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
Changelog
11/23/13 Removed 'baseurl' method for image path and plugin will now find the relative path with php instead (1.5)

6/15/13 Added CSS background functionality, added a callback option, and set default jpegQuality to 90 (1.4)

3/2/13   Fixed placeholder image bug: plugin will now switch to the original image if it's smaller than the breakpoint when using a placeholder image. Plugin also now fires on window load as well as resize (1.3)

12/22/12 Added a jpeg quality option (1.25)

12/20/12 Added support for pixel ratio, fixed an iOS bug (1.2)

11/26/12 Fixed image size up issue: plugin won't create new images that are larger than the original image (1.1)

10/20/12 Initial plugin (1)
*/

(function($) {

	$.fn.responsiveImg = function(additionalOptions) {

		var options = { //- set default options
			breakpoints : {
				"_small":360,
				"_medium":780,
				"_large":900
			},
			srcAttribute : "src",
			pathToPHP:"js",
			createNewImages:true,
			jpegQuality:90,
			callback:false
		}
		options = $.extend(options, additionalOptions ); //- override default options with user-supplied options

		var pathToPHP = options.pathToPHP; //- format path from image to php
		if (pathToPHP.charAt( pathToPHP.length-1 ) !== "/") {
			options["pathToPHP"] = pathToPHP+"/";
		}

		$(this).each(function() { //- do it for 'em all

			var $this = $(this); //- get this variable for later

			var src = $this.attr(options.srcAttribute); //- get the original src attribute so we can always come back to it
			var extension = src.split('.').pop(); //- get the file extension so we can add the suffix before the extension

			var breakpoints = options.breakpoints; //- create a new variable for breakpoints object
			var defaultBreakpoint = { "default_bp":1000000 }; //- set a "default" breakpoint for anything larger than the largest breakpoint
			breakpoints = $.extend(breakpoints,defaultBreakpoint);

			resizeImage($this,breakpoints,src,extension);

			$(window).on("resize load",function() {
				resizeImage($this,breakpoints,src,extension);
			});

		});

		function resizeImage($tag,breakpoints,src,extension) {

			var $this = $tag;
			var containerWidth = 0;

			if($this.prop("tagName") == "IMG"){ // find out if it's an image

				containerWidth = $this.parent().width(); //- get container width

				var cssMaxWidth = $this.css("maxWidth"); //- if we know an image's max width is a percentage, we can use smaller images because we know the maximum size is smaller than the container width
				if (cssMaxWidth.charAt( cssMaxWidth.length-1 ) == "%") {
					containerWidth *= parseInt(cssMaxWidth)*.01;
				} else {
					var percentageOfContainer = ( 100 * parseFloat($this.css('width')) / containerWidth ); //- account for max-width or width styles
					if (percentageOfContainer > 0 && percentageOfContainer < 100) {
						containerWidth *= percentageOfContainer*.01;
					}
				}

			}else{ // or anything else

				containerWidth = $this.width(); //- get container width

			}

			var breakpoint = breakpoints.default_bp; //- set default breakpoint (size when the page loaded)
			if (containerWidth > breakpoint) { breakpoint = $(window).width(); } //- account for sizing the window up

			var suffix = ""; //- set up the suffix variable to add to the img src

			if (window.devicePixelRatio >= 2) { //- account for retina and other pixel sizes
				containerWidth *= 2;
			} else if (window.devicePixelRatio >= 1.5) {
				containerWidth *= 1.5;
			}

			$.each(breakpoints,function(index,value) { //- loop through until we find the smallest "value" that's larger than the containerWidth
				if (value > containerWidth && value < breakpoint) {
					breakpoint = value;
					suffix = index;
				}
			});

			var newSrc = src.replace("."+extension,suffix+"."+extension); //- replace "dot extension" with "suffix dot extension"
			replaceImage($this,src,newSrc,breakpoint);

		}

		function replaceImage($tag,src,newSrc,size) {

			var $this = $tag;

			var img = new Image();
			$(img).on("load",function() { //- image exists
				setSource($this, newSrc); //- replace current image with suffixed image
			});
			$(img).on("error",function() { //- image doesn't exist
				if (options.createNewImages) {
					$.ajax({ //- ajax to a file to create a new image at the size we need
						url:options.pathToPHP+"responsiveImg.js.php",
						data:{ makeImage:1,fileIn:src,fileOut:newSrc,size:size,jpegQuality:options.jpegQuality },
						dataType:"html",
						success:function(data) {
							if (parseInt(data) > 0) {
								setSource($this, newSrc); //- use the newly created images
							} else {
								setSource($this, src); //- image would get sized up, so we didn't create it
							}

							// callback
							if (typeof options.callback == 'function') { // make sure the callback is a function
						    	options.callback.call(this); // brings the scope to the callback
							}
						},
						error:function() {
							// nothing
						}
					});
				}
			});
			img.src = newSrc; //- see if we get the image or get an error
		}

		function setSource($tag, src){
			var $this = $tag;
			if($this.prop("tagName") == "IMG"){ // find out if it's an image
				$this.attr("src",src);
			}else{ // or anything else
				$this.css({"background-image":"url('"+src+"')"});
			}
		}

		return this;
	};

})(jQuery);
