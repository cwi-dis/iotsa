#!/bin/bash

# This script generates a signing request for a iotsa key/certificate pair.
# This CSR is then given to igorCA for signing, and subsequently downloaded
# to the device.
# Run this, then sign, then run make-csr-step2.sh.
#
# 1024 or 512.  512 saves memory...
case x$1 in
x)
	HOSTNAME=iotsa.local
	;;
x*)
	HOSTNAME=$1
	;;
esac
BITS=1024

igorCA --keysize $BITS --remote gen tls.$HOSTNAME $HOSTNAME 192.168.4.1

openssl rsa -in tls.$HOSTNAME.key -outform DER -out tls.$HOSTNAME.key.der
openssl x509 -in tls.$HOSTNAME.crt -outform DER -out tls.$HOSTNAME.crt.der

KEY_B64=`base64 -i tls.$HOSTNAME.key.der`
CRT_B64=`base64 -i tls.$HOSTNAME.crt.der`
echo
echo  'Key (base64 DER): ' $KEY_B64
echo
echo  'Certificate (base64 DER): ' $CRT_B64
echo
echo Command line:
echo + iotsaControl --target $HOSTNAME --protocol https --noverify configWait config httpsKey="$KEY_B64" httpsCertificate="$CRT_B64" reboot=1
iotsaControl --target $HOSTNAME --protocol https --noverify configWait config httpsKey="$KEY_B64" httpsCertificate="$CRT_B64" reboot=1
