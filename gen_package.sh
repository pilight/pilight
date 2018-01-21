#!/usr/bin/env bash

#
# pilight core
#

ARCH=$1;
NAME="pilight"

if [ $# -eq 2 ] && [ "$2" = "debug" ]; then
	NAME="pilight-dbg"
fi

mkdir -p deb
mv *.deb deb
cd deb

DEBVER=$(git describe --always | sed -e 's/^v//g');
VERSION=$(echo $DEBVER | sed -e 's/-g.*//g');
WEBGUI=$(cat ../libs/webgui/version.txt)

dpkg -X pilight-8.0.9-Linux-pilight.deb source
dpkg -e pilight-8.0.9-Linux-pilight.deb control

mkdir -p source/etc/init.d
mkdir -p source/etc/systemd/system
mkdir -p source/etc/init
cp ../res/init/pilight.sysv source/etc/init.d/pilight
cp ../res/init/pilight.systemd source/etc/systemd/system/pilight.service
cp ../res/init/pilight.upstart source/etc/init/pilight.conf
cp ../res/pilight.pem source/etc/pilight/pilight.pem

cp ../res/deb/pilight/* control/
mv source/usr/local/lib/libpilight.so.* source/usr/local/lib/libpilight.so.$VERSION 2>/dev/null

SIZE=$(du -sk source | cut -f1)

if [ $# -eq 2 ] && [ "$2" = "debug" ]; then
	CONFLICTS="pilight"
else
	CONFLICTS="pilight-dbg"
fi
sed -i "s/@package@/$NAME/g" control/control
sed -i "s/@version@/$DEBVER/g" control/control
sed -i "s/@version@/$VERSION/g" control/postinst
sed -i "s/@version@/$VERSION/g" control/postrm
sed -i "s/@webgui@/$WEBGUI/g" control/control
sed -i "s/@arch@/$ARCH/g" control/control
sed -i "s/@size@/$SIZE/g" control/control
sed -i "s/@conflicts@/$CONFLICTS/g" control/control

if [ $# -eq 1 ] || [ "$2" != "debug" ]; then
	if [[ $ARCH =~ amd64 ]]; then
		find source/usr -type f -exec strip {} \; 2>/dev/null;
	elif [[ $ARCH =~ armhf ]]; then
		find source/usr -type f -exec arm-linux-gnueabihf-strip {} \; 2>/dev/null;
	elif [[ $ARCH =~ i386 ]]; then
		find source/usr -type f -exec strip {} \; 2>/dev/null;
	elif [[ $ARCH =~ mips ]]; then
		find source/usr -type f -exec mipsel-linux-gnu-strip {} \; 2>/dev/null;
	fi
fi

find source -type f -exec md5sum {} \; | sed -e 's@source/@@g' -e "s/ /  /g" > control/md5sums

tar -C control -czf control.tar.gz control md5sums postinst postrm preinst prerm 

chmod 755 source/usr/local/sbin/*
chmod 755 source/usr/local/bin/*
chmod 644 source/etc/pilight/*
chown -R 0:0 source/usr
chown -R 0:0 source/etc
tar -C source -czf data.tar.gz usr etc

echo "2.0" > debian-binary

ar rcv tmp.deb debian-binary control.tar.gz data.tar.gz

mv tmp.deb $NAME""$DEBVER"_"$ARCH".deb";

rm -r control source debian-binary control.tar.gz data.tar.gz 1>/dev/null 2>/dev/null

#
# pilight webgui
#

dpkg -X pilight-8.0.9-Linux-webgui.deb source
dpkg -e pilight-8.0.9-Linux-webgui.deb control

cp ../res/deb/webgui/* control/

SIZE=$(du -sk source | cut -f1)

sed -i "s/@version@/$DEBVER/g" control/control
sed -i "s/@webgui@/$WEBGUI/g" control/control
sed -i "s/@arch@/$ARCH/g" control/control
sed -i "s/@size@/$SIZE/g" control/control

find source -type f -exec md5sum {} \; | sed -e 's@source/@@g' -e "s/ /  /g" > control/md5sums

tar -C control -czf control.tar.gz control md5sums postinst postrm preinst prerm

chmod -R 755 source/usr/local/share/pilight/webgui/*
chown -R 33:33 source/usr/local/share/pilight/webgui/*

tar -C source -czf data.tar.gz usr

echo "2.0" > debian-binary

ar rcv tmp.deb debian-binary control.tar.gz data.tar.gz

mv tmp.deb "pilight-webgui"$WEBGUI"_"$ARCH".deb";

rm pilight-*-Linux-*.deb

rm -r control source debian-binary control.tar.gz data.tar.gz 1>/dev/null 2>/dev/null
