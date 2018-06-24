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
cat > tls.$HOSTNAME.reqconfig << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment

[req]
distinguished_name=req_distinguished_name
req_extensions = req_extensions

[req_distinguished_name]
countryName                     = Country Name (2 letter code)
stateOrProvinceName             = State or Province Name
0.organizationName              = Organization Name
commonName                      = Common Name

# Optionally, specify some defaults.
countryName_default             = NL
stateOrProvinceName_default     = Nederland
0.organizationName_default      = Alice Ltd
commonName_default = $HOSTNAME

[req_extensions]
subjectAltName = @alt_names

[alt_names]
DNS.1 = $HOSTNAME
DNS.2 = 192.168.4.1
EOF
openssl req -config tls.$HOSTNAME.reqconfig -new -key tls.$HOSTNAME.key -out tls.$HOSTNAME.csr
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
echo "Your CSR is in tls.$HOSTNAME.csr"
echo "Have this signed, possibly using ./make-csr-step2.sh or by submitting to the Igor CA."
echo "(The signing configuration for step2.sh (which you may want to adapt) is in tls.$HOSTNAME.csrconfig)"
echo "Put the result in tls.$HOSTNAME.crt in this directory and run ./make-csr-step3.sh"
echo
