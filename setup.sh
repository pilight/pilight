#!/bin/bash

cmake --help &>/dev/null || { echo "This program requires cmake"; exit; }
dialog -v &>/dev/null || { echo "This program requires dialog"; exit; }
dpkg --get-selections | grep -c libc6 &>/dev/null || { echo "This program requires libc6"; exit; }

function clean {
	rm -r CMakeFiles 2>/dev/null;
	rm cmake_install.cmake 2>/dev/null;
	rm cmake_uninstall.cmake 2>/dev/null;
	rm cmake_uninstall.cmake.in 2>/dev/null;
	rm cmake_postinstall.cmake 2>/dev/null;
	rm CMakeCache.txt 2>/dev/null;
	make clean 2>/dev/null;
	rm Makefile 2>/dev/null;
	rm install_manifest.txt 2>/dev/null;
	rm config.h 2>/dev/null;
	rm pilight-control 2>/dev/null;
	rm pilight-daemon 2>/dev/null;
	rm pilight-raw 2>/dev/null;
	rm pilight-send 2>/dev/null;
	rm pilight-learn 2>/dev/null;
	rm pilight-debug 2>/dev/null;
	rm pilight-receive 2>/dev/null;
	rm *.a* 2>/dev/null;
	rm *.so* 2>/dev/null;
}

if [ $# -eq 1 ]; then
	if [ "$1" == "clean" ] || [ "$1" == "clear" ]; then
		clean;
	fi
	if [ "$1" == "uninstall" ] || [ "$1" == "deinstall" ]; then
		make uninstall;
	fi
	if [ "$1" == "install" ]; then
		if [ ! -f Makefile ]; then
			cmake .
		fi
		make install;
	fi
	if [ "$1" == "help" ]; then
		echo -e "Usage: ./setup.sh [options]";
		echo -e "\t clean\t\tremove all cmake and make generated files";
		echo -e "\t install\tinstall pilight";
		echo -e "\t uninstall\tuninstall pilight";
	fi
else

	ORIGINALIFS=$IFS;
	IFS=$'\n';
	OPTIONS=($(cat CMakeConfig.txt));
	MENU=();
	for ROW in ${OPTIONS[@]}; do
		[[ "$ROW" =~ (.*\"(.*)\".*) ]] && DESC=${BASH_REMATCH[2]};
		[[ "$ROW" =~ (set\(([0-9A-Za-z_]+)[[:space:]].*) ]] && NAME=${BASH_REMATCH[2]};
		if [[ "$ROW" =~ " ON " ]]; then
			MENU+=($NAME $DESC on);
		else
			MENU+=($NAME $DESC off);
		fi
	done;

	OUTPUT=$(dialog --cancel-label "Abort" --extra-button --extra-label "Cancel and Install" --ok-label "Save and Install" --checklist "pilight configuration options" 17 100 10 ${MENU[@]} 3>&1 1>&2 2>&3);
	RETURN=$?;

	UPDATE=0;
	if [ $RETURN -eq 0 ]; then
		IFS=" ";
		RESULTS=($OUTPUT);
		IFS=$'\n';
		OPTIONS=($(cat CMakeConfig.txt));
		for ROW in ${OPTIONS[@]}; do
			MATCH=0;
			[[ "$ROW" =~ (set\(([0-9A-Za-z_]+)[[:space:]].*) ]] && NAME=${BASH_REMATCH[2]};
			for ROW1 in ${RESULTS[@]}; do	
				ROW1=${ROW1//\"/};
				if [ "$ROW1" == "$NAME" ]; then
					if [[ "$ROW" =~ " OFF " ]]; then
						UPDATE=1
						sed -i "s/\(.*\)\($NAME\) OFF \(.*\)/\1$NAME ON \3/" CMakeConfig.txt
					fi
					MATCH=1
				fi
			done;
			if [ $MATCH -eq 0 ]; then
				if [[ "$ROW" =~ " ON " ]]; then
					UPDATE=1
					sed -i "s/\(.*\)\($NAME\) ON \(.*\)/\1$NAME OFF \3/" CMakeConfig.txt
				fi
			fi
		done
	fi
	IFS=$ORIGINALIFS;
	if [ $RETURN -eq 1 ]; then
		exit;
	else
		if [ $UPDATE -eq 1 ]; then
			clean;
		fi
		cmake .
		make install
	fi
fi