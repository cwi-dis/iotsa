#!/bin/bash

# This script generates a ssigning request for a iotsa key/certificate pair.
#
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

openssl genrsa -out tls.$HOSTNAME.key $BITS
openssl req -new -key tls.$HOSTNAME.key -out tls.$HOSTNAME.csr
cat > tls.$HOSTNAME.csrconfig << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = $HOSTNAME
DNS.2 = 192.168.4.1
EOF
echo
echo Your CSR is in tls.$HOSTNAME.csr
echo "The signing configuration (which you may want to adapt) is in tls.$HOSTNAME.csrconfig"
echo "Have these signed, possibly using ./make-csr-step2.sh"
echo "Put the result in tls.$HOSTNAME.crt in this directory and run ./make-csr-step2.sh"
echo
