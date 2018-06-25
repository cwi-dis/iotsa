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

if [ ! -f tls.$HOSTNAME.key.pem ]; then
	echo Certificate tls.$HOSTNAME.key.pem not found. Did you run ./make-csr-step1.sh $1 first?
	exit 1
fi
if [ ! -f tls.$HOSTNAME.crt.pem ]; then
	echo Certificate tls.$HOSTNAME.crt.pem not found. Did you run ./make-csr-step2.sh $1 first?
	exit 1
fi

openssl rsa -in tls.$HOSTNAME.key.pem -outform DER -out tls.$HOSTNAME.key.der
openssl x509 -in tls.$HOSTNAME.crt.pem -outform DER -out tls.$HOSTNAME.crt.der

KEY_B64=`base64 -i tls.$HOSTNAME.key.der`
CRT_B64=`base64 -i tls.$HOSTNAME.crt.der`
echo
echo  'Key (base64 DER): ' $KEY_B64
echo
echo  'Certificate (base64 DER): ' $CRT_B64
echo
echo Command line:
echo iotsaControl --target $HOSTNAME --protocol https --noverify configWait config httpsKey="$KEY_B64" httpsCertificate="$CRT_B64" reboot=1
