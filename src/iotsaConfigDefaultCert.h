// Default key and certificate for https service
// The certificate is stored in PMEM.
// Regenerate with extras/make-self-signed-cert.sh and copy the key and
// certificate date in here.
static const uint8_t defaultHttpsCertificate[] PROGMEM = {

  0x30, 0x82, 0x03, 0x3c, 0x30, 0x82, 0x02, 0x24, 0xa0, 0x03, 0x02, 0x01,
  0x02, 0x02, 0x14, 0x36, 0x20, 0x02, 0x6a, 0xfa, 0x64, 0x4e, 0xa2, 0xb2,
  0x20, 0x10, 0x57, 0xe9, 0xb5, 0x71, 0x68, 0xf7, 0xe5, 0xbd, 0x97, 0x30,
  0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
  0x05, 0x00, 0x30, 0x36, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55, 0x04,
  0x0a, 0x0c, 0x15, 0x63, 0x77, 0x69, 0x2d, 0x64, 0x69, 0x73, 0x2d, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x2d, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74,
  0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x2e, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x30, 0x1e,
  0x17, 0x0d, 0x32, 0x34, 0x30, 0x37, 0x32, 0x31, 0x31, 0x32, 0x35, 0x37,
  0x32, 0x33, 0x5a, 0x17, 0x0d, 0x33, 0x38, 0x30, 0x33, 0x33, 0x30, 0x31,
  0x32, 0x35, 0x37, 0x32, 0x33, 0x5a, 0x30, 0x36, 0x31, 0x1e, 0x30, 0x1c,
  0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x15, 0x63, 0x77, 0x69, 0x2d, 0x64,
  0x69, 0x73, 0x2d, 0x69, 0x6f, 0x74, 0x73, 0x61, 0x2d, 0x70, 0x72, 0x6f,
  0x6a, 0x65, 0x63, 0x74, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0b, 0x69, 0x6f, 0x74, 0x73, 0x61, 0x2e, 0x6c, 0x6f, 0x63,
  0x61, 0x6c, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
  0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01,
  0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xd1,
  0xc9, 0x64, 0xb1, 0xd0, 0xc3, 0x38, 0xe7, 0x9a, 0x32, 0x26, 0x6b, 0xf3,
  0x32, 0x8e, 0x04, 0xf3, 0xbe, 0x5e, 0x8d, 0xfe, 0x51, 0x4a, 0x23, 0x3f,
  0xce, 0x6c, 0x1a, 0xc1, 0x3c, 0xbc, 0x5c, 0x9c, 0x76, 0xe1, 0xdd, 0xd0,
  0x4b, 0xcb, 0x81, 0x34, 0xd1, 0x10, 0x81, 0xac, 0xc9, 0x5a, 0xba, 0xa6,
  0xb1, 0xd5, 0xe7, 0x40, 0x90, 0xea, 0xe0, 0xd9, 0x5c, 0x56, 0xa2, 0x67,
  0xb0, 0xbe, 0x5a, 0x8d, 0x0d, 0xac, 0x10, 0x71, 0x43, 0xa9, 0x6c, 0x49,
  0x94, 0x6b, 0x85, 0xbc, 0xeb, 0x27, 0x47, 0xd4, 0xc9, 0xe7, 0x1f, 0xa8,
  0x8a, 0x74, 0xee, 0xcc, 0x21, 0xda, 0x89, 0x66, 0xec, 0x33, 0x5b, 0xbc,
  0x75, 0x23, 0xe2, 0x62, 0xff, 0x99, 0xe4, 0x9f, 0x5b, 0x46, 0xa1, 0x77,
  0x36, 0xbd, 0x6b, 0x14, 0xb1, 0xa1, 0x03, 0x31, 0x72, 0xee, 0xa5, 0xc5,
  0x3a, 0x58, 0x33, 0xae, 0xb4, 0xf8, 0x18, 0x1b, 0xf1, 0xcd, 0xc5, 0x72,
  0xb0, 0x73, 0xb0, 0x14, 0x8b, 0xed, 0x3e, 0x04, 0x05, 0xc6, 0x5e, 0xd5,
  0x99, 0xf8, 0x9c, 0x94, 0xcb, 0xde, 0x77, 0xa0, 0x6a, 0xa3, 0x3a, 0x3e,
  0x5a, 0xda, 0xb7, 0xd7, 0xb1, 0x61, 0x96, 0xe3, 0x91, 0xaa, 0xc6, 0x34,
  0xfb, 0xb3, 0x96, 0x39, 0x81, 0xd0, 0x76, 0x1e, 0xdc, 0x08, 0x7e, 0x38,
  0x8f, 0x4c, 0x40, 0x04, 0xee, 0xc1, 0x74, 0xa4, 0x15, 0xa7, 0x81, 0x0d,
  0x72, 0x47, 0x37, 0xe8, 0x17, 0x32, 0x98, 0xb9, 0x84, 0x58, 0x87, 0xb6,
  0x1f, 0x55, 0x56, 0x11, 0xd3, 0xd5, 0xfa, 0xb7, 0xe8, 0x64, 0xb5, 0xcd,
  0x54, 0xbd, 0xdf, 0x60, 0xb2, 0xa5, 0x0f, 0xf4, 0x52, 0xee, 0x09, 0xd9,
  0x07, 0xc7, 0x22, 0xf3, 0xe1, 0xe3, 0x28, 0x8b, 0x1b, 0x7e, 0x81, 0xf6,
  0x4d, 0x5f, 0xa0, 0x37, 0xdc, 0xce, 0xd8, 0xc9, 0xa4, 0x28, 0x62, 0x36,
  0x2e, 0xe6, 0x8f, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x42, 0x30, 0x40,
  0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x2d,
  0x88, 0x97, 0xeb, 0xab, 0x1d, 0xc9, 0x7c, 0x16, 0xed, 0x52, 0xe2, 0x2b,
  0x82, 0xb9, 0xfd, 0x10, 0x25, 0x65, 0x0a, 0x30, 0x1f, 0x06, 0x03, 0x55,
  0x1d, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x52, 0x19, 0xa1, 0x30,
  0x6c, 0xfc, 0x38, 0xa9, 0x85, 0xa5, 0xda, 0x8e, 0x0a, 0x27, 0x72, 0xe0,
  0xd0, 0xf1, 0x9d, 0xd9, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
  0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00,
  0x8f, 0x57, 0x6a, 0x80, 0x6f, 0x2d, 0x8d, 0x63, 0xae, 0x83, 0xdc, 0xb5,
  0x6b, 0x4f, 0x05, 0x32, 0x45, 0xb5, 0xa4, 0x48, 0x93, 0x48, 0xd6, 0x85,
  0x42, 0x60, 0xdf, 0x99, 0x64, 0x0c, 0x0a, 0xd5, 0x02, 0x5b, 0x2d, 0x33,
  0xee, 0xfc, 0xeb, 0x1a, 0x66, 0xd6, 0xdc, 0xc1, 0xd5, 0xb6, 0x3a, 0x9d,
  0xf0, 0x11, 0x1e, 0xbd, 0x34, 0x91, 0x37, 0xa7, 0xac, 0x01, 0xb0, 0x07,
  0xda, 0xaa, 0x11, 0x28, 0xdb, 0x7d, 0x64, 0x2f, 0xb3, 0x06, 0xce, 0x68,
  0xeb, 0x65, 0xba, 0xbb, 0x3a, 0x13, 0x03, 0x9e, 0x17, 0x74, 0xa0, 0x62,
  0x13, 0x3b, 0x84, 0xf0, 0x7b, 0x1e, 0x1a, 0x86, 0xab, 0x6c, 0xde, 0x86,
  0x18, 0xd6, 0xdc, 0x74, 0x24, 0xc4, 0xd9, 0x5f, 0x93, 0x2a, 0x3c, 0x92,
  0x5d, 0x01, 0xd2, 0xb6, 0x6c, 0x03, 0x97, 0xda, 0x82, 0xe0, 0x1f, 0xf5,
  0x4a, 0x8b, 0x8f, 0xfa, 0x91, 0x76, 0x90, 0x8d, 0x01, 0xf8, 0x75, 0x08,
  0x4d, 0x69, 0x33, 0xdd, 0x54, 0x16, 0x40, 0xd9, 0x51, 0x2e, 0xbb, 0xc5,
  0xd7, 0xde, 0xe6, 0x8b, 0xbd, 0x2e, 0xf5, 0x18, 0xfd, 0x27, 0xd6, 0x78,
  0x94, 0x6f, 0xb5, 0xaf, 0x30, 0xb3, 0x0f, 0xe2, 0x0b, 0xc1, 0x9b, 0x39,
  0xb7, 0x10, 0xcf, 0x0c, 0x2e, 0x64, 0x38, 0x8a, 0xdb, 0xba, 0xaa, 0x10,
  0x64, 0x1c, 0x05, 0xec, 0x2f, 0x13, 0x45, 0x44, 0xb4, 0xab, 0xfe, 0x44,
  0xbe, 0x8a, 0x43, 0xc2, 0xc3, 0x5e, 0x7c, 0xca, 0xde, 0x2d, 0xb6, 0xc2,
  0x20, 0xac, 0x52, 0xff, 0x97, 0x45, 0x4c, 0x70, 0x17, 0xa2, 0xff, 0x6d,
  0xeb, 0xbe, 0xe5, 0x6c, 0x11, 0xf5, 0x6d, 0x28, 0x28, 0x16, 0x5d, 0x68,
  0xf3, 0x1e, 0x8a, 0x1a, 0x25, 0xc4, 0xdd, 0x1c, 0xab, 0x37, 0x0f, 0x12,
  0x7c, 0xfb, 0xaa, 0x0c, 0x0d, 0x05, 0xe3, 0x9d, 0xd8, 0x9c, 0xc7, 0x61,
  0x14, 0x36, 0x09, 0x9b
};

// And so is the key.  These could also be in DRAM
static const uint8_t defaultHttpsKey[] PROGMEM = {

  0x30, 0x82, 0x04, 0xbd, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
  0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
  0x04, 0xa7, 0x30, 0x82, 0x04, 0xa3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01,
  0x01, 0x00, 0xd1, 0xc9, 0x64, 0xb1, 0xd0, 0xc3, 0x38, 0xe7, 0x9a, 0x32,
  0x26, 0x6b, 0xf3, 0x32, 0x8e, 0x04, 0xf3, 0xbe, 0x5e, 0x8d, 0xfe, 0x51,
  0x4a, 0x23, 0x3f, 0xce, 0x6c, 0x1a, 0xc1, 0x3c, 0xbc, 0x5c, 0x9c, 0x76,
  0xe1, 0xdd, 0xd0, 0x4b, 0xcb, 0x81, 0x34, 0xd1, 0x10, 0x81, 0xac, 0xc9,
  0x5a, 0xba, 0xa6, 0xb1, 0xd5, 0xe7, 0x40, 0x90, 0xea, 0xe0, 0xd9, 0x5c,
  0x56, 0xa2, 0x67, 0xb0, 0xbe, 0x5a, 0x8d, 0x0d, 0xac, 0x10, 0x71, 0x43,
  0xa9, 0x6c, 0x49, 0x94, 0x6b, 0x85, 0xbc, 0xeb, 0x27, 0x47, 0xd4, 0xc9,
  0xe7, 0x1f, 0xa8, 0x8a, 0x74, 0xee, 0xcc, 0x21, 0xda, 0x89, 0x66, 0xec,
  0x33, 0x5b, 0xbc, 0x75, 0x23, 0xe2, 0x62, 0xff, 0x99, 0xe4, 0x9f, 0x5b,
  0x46, 0xa1, 0x77, 0x36, 0xbd, 0x6b, 0x14, 0xb1, 0xa1, 0x03, 0x31, 0x72,
  0xee, 0xa5, 0xc5, 0x3a, 0x58, 0x33, 0xae, 0xb4, 0xf8, 0x18, 0x1b, 0xf1,
  0xcd, 0xc5, 0x72, 0xb0, 0x73, 0xb0, 0x14, 0x8b, 0xed, 0x3e, 0x04, 0x05,
  0xc6, 0x5e, 0xd5, 0x99, 0xf8, 0x9c, 0x94, 0xcb, 0xde, 0x77, 0xa0, 0x6a,
  0xa3, 0x3a, 0x3e, 0x5a, 0xda, 0xb7, 0xd7, 0xb1, 0x61, 0x96, 0xe3, 0x91,
  0xaa, 0xc6, 0x34, 0xfb, 0xb3, 0x96, 0x39, 0x81, 0xd0, 0x76, 0x1e, 0xdc,
  0x08, 0x7e, 0x38, 0x8f, 0x4c, 0x40, 0x04, 0xee, 0xc1, 0x74, 0xa4, 0x15,
  0xa7, 0x81, 0x0d, 0x72, 0x47, 0x37, 0xe8, 0x17, 0x32, 0x98, 0xb9, 0x84,
  0x58, 0x87, 0xb6, 0x1f, 0x55, 0x56, 0x11, 0xd3, 0xd5, 0xfa, 0xb7, 0xe8,
  0x64, 0xb5, 0xcd, 0x54, 0xbd, 0xdf, 0x60, 0xb2, 0xa5, 0x0f, 0xf4, 0x52,
  0xee, 0x09, 0xd9, 0x07, 0xc7, 0x22, 0xf3, 0xe1, 0xe3, 0x28, 0x8b, 0x1b,
  0x7e, 0x81, 0xf6, 0x4d, 0x5f, 0xa0, 0x37, 0xdc, 0xce, 0xd8, 0xc9, 0xa4,
  0x28, 0x62, 0x36, 0x2e, 0xe6, 0x8f, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02,
  0x82, 0x01, 0x00, 0x67, 0xd0, 0x89, 0x4f, 0x83, 0x57, 0x30, 0x80, 0x50,
  0x2a, 0x94, 0x1f, 0xe4, 0x94, 0x36, 0x7a, 0x95, 0xe2, 0x35, 0xa9, 0x7f,
  0xda, 0x5b, 0x5d, 0x51, 0x34, 0x86, 0x03, 0xc1, 0x6f, 0x9c, 0x69, 0x1a,
  0x16, 0xac, 0x94, 0x57, 0x81, 0x42, 0x9e, 0x58, 0x74, 0x42, 0xfe, 0x60,
  0xf2, 0xad, 0xbe, 0xe3, 0x41, 0xea, 0xf0, 0x0a, 0xe0, 0x0c, 0x13, 0xa4,
  0x0c, 0xd1, 0x64, 0x81, 0xfa, 0x91, 0x60, 0x7f, 0x1f, 0xee, 0x6f, 0x9e,
  0x95, 0x54, 0xb0, 0x9d, 0x42, 0xe0, 0xe8, 0xa8, 0x42, 0x18, 0x48, 0xb0,
  0x31, 0x0f, 0xfa, 0x77, 0x2d, 0xb9, 0x4a, 0xf8, 0xe2, 0xa1, 0xe2, 0x4f,
  0xf8, 0x00, 0x3c, 0x5b, 0xe8, 0xfe, 0x0e, 0x88, 0x5e, 0x23, 0x9a, 0x63,
  0x0f, 0xc8, 0xc6, 0x9b, 0x97, 0x8f, 0xa9, 0xff, 0x6e, 0x1d, 0x97, 0x56,
  0x6f, 0xa1, 0x50, 0x90, 0x66, 0xdc, 0x82, 0x0a, 0xe9, 0x4c, 0x41, 0xbf,
  0xc2, 0x49, 0xf5, 0x7e, 0x2c, 0xc1, 0xe3, 0x5e, 0x9a, 0xd4, 0x03, 0x5f,
  0x2b, 0xa5, 0x0b, 0xed, 0xfa, 0xb9, 0x5b, 0xc0, 0x7d, 0x66, 0x25, 0xe4,
  0x49, 0xbe, 0x12, 0xb9, 0x29, 0x27, 0x4a, 0xf6, 0x6e, 0xd4, 0x72, 0x3e,
  0xd2, 0x62, 0x72, 0x19, 0xd7, 0xe8, 0x64, 0x59, 0x64, 0x85, 0x6c, 0xc0,
  0xa5, 0xf5, 0x3c, 0x75, 0x8a, 0xa1, 0x86, 0x20, 0x43, 0xf7, 0x06, 0x0f,
  0x90, 0x72, 0xb6, 0x2d, 0xc6, 0x68, 0x0a, 0xb5, 0x01, 0x96, 0x77, 0x23,
  0x1f, 0x4d, 0x86, 0xe5, 0x49, 0x40, 0x52, 0xc8, 0x2e, 0x04, 0x81, 0x6d,
  0x31, 0xac, 0x38, 0xc2, 0x65, 0x5b, 0xc4, 0x1d, 0xdc, 0x8b, 0x1c, 0x6a,
  0x0d, 0x5d, 0x5d, 0xa6, 0xbd, 0xc2, 0xcf, 0xaf, 0xfa, 0x5c, 0x74, 0x93,
  0x40, 0xb1, 0x16, 0xc1, 0xc6, 0xba, 0x4d, 0x2a, 0xd1, 0x01, 0x1a, 0x11,
  0xbc, 0xd3, 0x35, 0x51, 0x75, 0x0f, 0x51, 0x02, 0x81, 0x81, 0x00, 0xfd,
  0x39, 0x8b, 0xce, 0xec, 0x8d, 0x15, 0xd5, 0xad, 0x38, 0x20, 0x9a, 0x99,
  0x7b, 0x2f, 0xe8, 0x3f, 0x31, 0x3e, 0xeb, 0xa8, 0xdb, 0x0e, 0x54, 0xf7,
  0xc9, 0x3f, 0x75, 0x52, 0x13, 0x2f, 0xbe, 0x6f, 0xb6, 0x6b, 0xff, 0x0d,
  0x8d, 0x65, 0xb1, 0xf3, 0x26, 0x45, 0xee, 0xab, 0xec, 0xe5, 0x76, 0xcf,
  0x50, 0x79, 0x68, 0xe9, 0x7d, 0x6f, 0xa5, 0x47, 0x63, 0xd4, 0x85, 0x79,
  0x6a, 0x71, 0x2f, 0x42, 0xb8, 0x6c, 0x25, 0x85, 0xa9, 0x2b, 0xd6, 0xea,
  0x21, 0x6f, 0x75, 0xc7, 0xfb, 0x67, 0x3b, 0x10, 0x5f, 0xdc, 0xa2, 0x8d,
  0x22, 0xc4, 0xe2, 0x8b, 0x44, 0x55, 0xc6, 0x61, 0x89, 0x3d, 0x82, 0xc8,
  0xd0, 0xc0, 0xd1, 0x49, 0x7b, 0x43, 0x47, 0xac, 0x80, 0x40, 0x5e, 0x05,
  0xa8, 0x84, 0xe0, 0x05, 0xf5, 0x40, 0x40, 0x8a, 0x03, 0xd6, 0xe1, 0x31,
  0x42, 0x79, 0x62, 0x90, 0xd8, 0xe0, 0xdf, 0x02, 0x81, 0x81, 0x00, 0xd4,
  0x15, 0xf9, 0xe7, 0x86, 0x70, 0xa8, 0xec, 0x80, 0xec, 0xb2, 0x5a, 0xeb,
  0x96, 0xcf, 0x28, 0x7c, 0x08, 0x12, 0xed, 0x90, 0xab, 0x91, 0xd3, 0x45,
  0x0d, 0x56, 0xd5, 0xfd, 0xae, 0xa9, 0xd5, 0x2c, 0x46, 0x6a, 0x33, 0x3c,
  0x02, 0x22, 0xef, 0x86, 0xb1, 0x6f, 0x90, 0x0e, 0x3d, 0x68, 0x4e, 0x2f,
  0xc1, 0x91, 0x4c, 0x4c, 0x12, 0xcd, 0xf3, 0xb4, 0xd8, 0xe2, 0x4d, 0x29,
  0x99, 0x61, 0xe5, 0x17, 0xfe, 0xd1, 0xbc, 0x9d, 0xdb, 0xab, 0x0d, 0x46,
  0x9a, 0x70, 0xc1, 0x3c, 0x60, 0x4f, 0x9f, 0x4d, 0x3d, 0x9d, 0x10, 0xaf,
  0x74, 0x3c, 0x07, 0x08, 0xf2, 0x02, 0xdd, 0xeb, 0x99, 0xa8, 0xb6, 0x8e,
  0xfd, 0x24, 0xa7, 0x00, 0xad, 0x17, 0x64, 0x3c, 0xda, 0xdd, 0xa0, 0x52,
  0x17, 0x6d, 0x47, 0x0f, 0xb4, 0x99, 0x54, 0x80, 0x39, 0xb0, 0x66, 0x4d,
  0xbe, 0xae, 0x97, 0x49, 0xa8, 0x40, 0x51, 0x02, 0x81, 0x80, 0x1f, 0x6b,
  0xb8, 0x91, 0x60, 0x80, 0xd1, 0x28, 0xc6, 0x69, 0xa4, 0x82, 0x0a, 0x71,
  0x62, 0xac, 0x6a, 0xca, 0xed, 0x87, 0xc8, 0x58, 0x06, 0x1b, 0x3c, 0xf3,
  0xd7, 0xcf, 0xf2, 0xf3, 0x36, 0x85, 0x66, 0xcf, 0x37, 0xef, 0x59, 0xfb,
  0x25, 0x97, 0x43, 0x18, 0x88, 0xac, 0xe8, 0xe0, 0x68, 0x48, 0xa9, 0xc8,
  0xce, 0x87, 0xda, 0x11, 0x1a, 0x7d, 0x63, 0xb2, 0x5b, 0x78, 0x84, 0x6c,
  0x54, 0xc7, 0x0c, 0x7c, 0x5d, 0xce, 0xfa, 0x1a, 0xd7, 0xb9, 0xbf, 0x2b,
  0x8e, 0xed, 0x0a, 0x77, 0x83, 0x83, 0xac, 0xb0, 0x78, 0x6c, 0x23, 0x1f,
  0x21, 0x57, 0x0a, 0xf8, 0xdb, 0xbb, 0xd5, 0xf6, 0x75, 0x8f, 0x78, 0xe3,
  0x8e, 0x49, 0x69, 0xd8, 0xc5, 0xac, 0x6b, 0x17, 0x2e, 0xe5, 0x7b, 0xc6,
  0x41, 0x56, 0x9a, 0xe6, 0xa7, 0xa6, 0x5a, 0x79, 0xc7, 0x7c, 0x01, 0xe4,
  0xc1, 0xb3, 0x1d, 0x4e, 0x0b, 0xf9, 0x02, 0x81, 0x81, 0x00, 0xab, 0x0f,
  0xbe, 0xad, 0x9b, 0xa6, 0x2a, 0xd9, 0xf7, 0x72, 0xf2, 0xb8, 0x9a, 0xe4,
  0xdc, 0xda, 0x0e, 0x90, 0x84, 0x93, 0xd0, 0xe8, 0x51, 0x8a, 0x52, 0x5b,
  0xdb, 0xfa, 0x65, 0xcf, 0x07, 0x74, 0xc5, 0x6d, 0x56, 0x23, 0x54, 0xf7,
  0x74, 0x2e, 0x36, 0x39, 0xcf, 0x7e, 0x25, 0xbe, 0x29, 0xef, 0x46, 0x5d,
  0x9e, 0x50, 0x27, 0xdb, 0xd2, 0xfa, 0x0a, 0x98, 0x14, 0x8f, 0xa0, 0x49,
  0xf2, 0xc7, 0xd1, 0x7e, 0xda, 0xb4, 0x83, 0xae, 0xb6, 0x5c, 0xd2, 0xe1,
  0xa6, 0xa8, 0x75, 0x86, 0x49, 0x32, 0x78, 0x1e, 0x86, 0x1c, 0xfb, 0x27,
  0x89, 0x73, 0x33, 0x27, 0xe0, 0x60, 0x16, 0xb3, 0xad, 0x7c, 0xf2, 0x00,
  0x04, 0x1c, 0x1f, 0x53, 0x7d, 0x56, 0x80, 0x2f, 0x18, 0x2b, 0x43, 0x8b,
  0x59, 0xbc, 0xc3, 0x52, 0x2b, 0x8a, 0x18, 0x92, 0x6f, 0x51, 0x54, 0xcb,
  0xc4, 0x0e, 0x8a, 0x7e, 0x86, 0xc1, 0x02, 0x81, 0x80, 0x62, 0xef, 0xd3,
  0x51, 0x10, 0x30, 0x02, 0x69, 0x31, 0xf3, 0x02, 0x88, 0x2b, 0x94, 0xbe,
  0x44, 0x4f, 0xa6, 0xa7, 0x38, 0x84, 0x24, 0x13, 0xbb, 0x15, 0x20, 0x38,
  0x20, 0xdd, 0x80, 0xa8, 0x01, 0x5d, 0x8e, 0x7a, 0x47, 0x4f, 0xa2, 0x53,
  0xc3, 0x09, 0x84, 0x87, 0xd4, 0xcc, 0xe0, 0x01, 0x11, 0xac, 0x1f, 0x49,
  0x3e, 0x5d, 0x87, 0x0d, 0x33, 0xd9, 0x26, 0x91, 0x5b, 0x2e, 0xfa, 0xfa,
  0xe4, 0xa4, 0x4c, 0x9c, 0x5c, 0x0e, 0x63, 0x41, 0xdb, 0x4b, 0x62, 0x5b,
  0x7a, 0x7c, 0x43, 0x95, 0xbb, 0x3a, 0xb9, 0x01, 0x3f, 0xc8, 0x2d, 0xc2,
  0x4a, 0x86, 0x8c, 0xec, 0x67, 0xa4, 0xe0, 0x1e, 0x9d, 0x63, 0xf6, 0x14,
  0x04, 0xdb, 0xcf, 0x6b, 0x14, 0xd1, 0xdf, 0xa2, 0x4d, 0x14, 0xcc, 0x18,
  0x1d, 0x30, 0xfc, 0xaa, 0x37, 0x53, 0x52, 0xc9, 0x3d, 0x86, 0x7c, 0x1b,
  0xd4, 0xb3, 0x62, 0xaa, 0xa3
};