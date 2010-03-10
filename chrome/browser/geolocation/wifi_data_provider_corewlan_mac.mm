// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements a WLAN API binding for CoreWLAN, as available on OSX 10.6

#include "chrome/browser/geolocation/wifi_data_provider_mac.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>

#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"

// Define a subset of the CoreWLAN interfaces we require. We can't depend on
// CoreWLAN.h existing as we need to build on 10.5 SDKs. We can't just send
// messages to an untyped id due to the build treating warnings as errors,
// hence the reason we need class definitions.
// TODO(joth): When we build all 10.6 code exclusively 10.6 SDK (or later)
// tidy this up to use the framework directly. See http://crbug.com/37703

@interface CWInterface : NSObject
+ (CWInterface*)interface;
- (NSArray*)scanForNetworksWithParameters:(NSDictionary*)parameters
                                    error:(NSError**)error;
@end

@interface CWNetwork : NSObject <NSCopying, NSCoding>
@property(readonly) NSString* ssid;
@property(readonly) NSString* bssid;
@property(readonly) NSData* bssidData;
@property(readonly) NSNumber* securityMode;
@property(readonly) NSNumber* phyMode;
@property(readonly) NSNumber* channel;
@property(readonly) NSNumber* rssi;
@property(readonly) NSNumber* noise;
@property(readonly) NSData* ieData;
@property(readonly) BOOL isIBSS;
- (BOOL)isEqualToNetwork:(CWNetwork*)network;
@end

class CoreWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  CoreWlanApi() {}

  // Must be called before any other interface method. Will return false if the
  // CoreWLAN framework cannot be initialized (e.g. running on pre-10.6 OSX),
  // in which case no other method may be called.
  bool Init();

  // WlanApiInterface
  virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data);

 private:
  scoped_nsobject<NSBundle> bundle_;
  scoped_nsobject<CWInterface> corewlan_interface_;
  scoped_nsobject<NSString> merge_key_;

  DISALLOW_COPY_AND_ASSIGN(CoreWlanApi);
};

bool CoreWlanApi::Init() {
  // As the WLAN api binding runs on its own thread, we need to provide our own
  // auto release pool. It's simplest to do this as an automatic variable in
  // each method that needs it, to ensure the scoping is correct and does not
  // interfere with any other code using autorelease pools on the thread.
  base::ScopedNSAutoreleasePool auto_pool;
  bundle_.reset([[NSBundle alloc]
      initWithPath:@"/System/Library/Frameworks/CoreWLAN.framework"]);
  if (!bundle_) {
    DLOG(INFO) << "Failed to load the CoreWLAN framework bundle";
    return false;
  }
  corewlan_interface_.reset([[bundle_ classNamed:@"CWInterface"] interface]);
  if (!corewlan_interface_) {
    DLOG(INFO) << "Failed to create the CWInterface instance";
    return false;
  }
  [corewlan_interface_ retain];

  // Dynamically look up the value of the kCWScanKeyMerge (i.e. without build
  // time dependency on the 10.6 specific library).
  void* dl_handle = dlopen([[bundle_ executablePath] fileSystemRepresentation],
                           RTLD_LAZY | RTLD_LOCAL);
  if (dl_handle) {
    const NSString* key = *reinterpret_cast<const NSString**>(
        dlsym(dl_handle, "kCWScanKeyMerge"));
    if (key)
      merge_key_.reset([key copy]);
  }
  // "Leak" dl_handle rather than dlclose it, to ensure |merge_key_|
  // remains valid.
  if (!merge_key_.get()) {
    // Fall back to a known-working value should the lookup fail (if
    // this value is itself wrong it's not the end of the world, we might just
    // get very slightly lower quality location fixes due to SSID merges).
    DLOG(WARNING) << "Could not dynamically load the CoreWLAN merge key";
    merge_key_.reset([@"SCAN_MERGE" retain]);
  }

  return true;
}

bool CoreWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  base::ScopedNSAutoreleasePool auto_pool;
  DCHECK(corewlan_interface_);
  NSError* err = nil;
  // Initialize the scan parameters with scan key merging disabled, so we get
  // every AP listed in the scan without any SSID de-duping logic.
  NSDictionary* params =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO]
                                  forKey:merge_key_.get()];

  NSArray* scan = [corewlan_interface_ scanForNetworksWithParameters:params
                                                               error:&err];

  const int error_code = [err code];
  const int count = [scan count];
  if (error_code && !count) {
    DLOG(WARNING) << "CoreWLAN scan failed " << error_code;
    return false;
  }
  DLOG(INFO) << "Found " << count << " wifi APs";

  for (CWNetwork* network in scan) {
    DCHECK(network);
    AccessPointData access_point_data;
    NSData* mac = [network bssidData];
    DCHECK([mac length] == 6);
    access_point_data.mac_address = MacAddressAsString16(
        static_cast<const uint8*>([mac bytes]));
    access_point_data.radio_signal_strength = [[network rssi] intValue];
    access_point_data.channel = [[network channel] intValue];
    access_point_data.signal_to_noise =
        access_point_data.radio_signal_strength - [[network noise] intValue];
    access_point_data.ssid = UTF8ToUTF16([[network ssid] UTF8String]);
    data->insert(access_point_data);
  }
  return true;
}

WifiDataProviderCommon::WlanApiInterface* NewCoreWlanApi() {
  scoped_ptr<CoreWlanApi> self(new CoreWlanApi);
  if (self->Init())
    return self.release();

  return NULL;
}
