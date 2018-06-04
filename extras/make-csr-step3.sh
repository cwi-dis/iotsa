#!/bin/bash

# This script generates a self-signed certificate for use by the ESP8266
# Replace your-name-here with somethine appropriate before running and use
# the generated .H files in your code as follows:
#
#      static const uint8_t rsakey[]  ICACHE_RODATA_ATTR = {
#        #include "key.h"
#      };
#
#      static const uint8_t x509[] ICACHE_RODATA_ATTR = {
#        #include "x509.h"
#      };
#
#      ....
#      WiFiServerSecure server(443);
#      server.setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));
#      ....

# 1024 or 512.  512 saves memory...
case x$1 in
x)
	HOSTNAME=iotsa.local
	;;
x*)
	HOSTNAME=$1
	;;
esac

if [ ! -f tls.$HOSTNAME.key ]; then
	echo Certificate tls.$HOSTNAME.key not found. Did you run ./make-csr-step1.sh $1 first?
	exit 1
fi
if [ ! -f tls.$HOSTNAME.crt ]; then
	echo Certificate tls.$HOSTNAME.crt not found. Did you run ./make-csr-step2.sh $1 first?
	exit 1
fi

# xxd -i tls.$HOSTNAME.key   | sed 's/.*{//' | sed 's/\};//' | sed 's/unsigned.*//' > "key.h"
# xxd -i tls.$HOSTNAME.crt  | sed 's/.*{//' | sed 's/\};//' | sed 's/unsigned.*//' > "x509.h"
KEY_B64=`base64 -i tls.$HOSTNAME.key`
CRT_B64=`base64 -i tls.$HOSTNAME.crt`
echo
echo Hex in key.h and x509.h
echo
echo  'Key (base64 DER): ' $KEY_B64
echo
echo  'Certificate (base64 DER): ' $CRT_B64
echo
echo Command line:
echo iotsaControl --target $HOSTNAME --protocol https --noverify configWait config httpsKey="$KEY_B64" httpsCertificate="$CRT_B64" reboot=1
