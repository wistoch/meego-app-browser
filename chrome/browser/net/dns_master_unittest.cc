// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/common/net/dns.h"
#include "net/base/address_list.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

class WaitForResolutionHelper;

typedef base::RepeatingTimer<WaitForResolutionHelper> HelperTimer;

class WaitForResolutionHelper {
 public:
  WaitForResolutionHelper(DnsMaster* master, const NameList& hosts,
                          HelperTimer* timer)
      : master_(master),
        hosts_(hosts),
        timer_(timer) {
  }

  void Run() {
    for (NameList::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i)
      if (master_->GetResolutionDuration(net::HostPortPair(*i, 80)) ==
          DnsHostInfo::kNullDuration)
        return;  // We don't have resolution for that host.

    // When all hostnames have been resolved, exit the loop.
    timer_->Stop();
    MessageLoop::current()->Quit();
    delete timer_;
    delete this;
  }

 private:
  DnsMaster* master_;
  const NameList hosts_;
  HelperTimer* timer_;
};

class DnsMasterTest : public testing::Test {
 public:
  DnsMasterTest()
      : io_thread_(ChromeThread::IO, &loop_),
        host_resolver_(new net::MockCachingHostResolver()),
        default_max_queueing_delay_(TimeDelta::FromMilliseconds(
            DnsGlobalInit::kMaxPrefetchQueueingDelayMs)) {
  }

 protected:
  virtual void SetUp() {
#if defined(OS_WIN)
    net::EnsureWinsockInit();
#endif
    // Since we are using a caching HostResolver, the following latencies will
    // only be incurred by the first request, after which the result will be
    // cached internally by |host_resolver_|.
    net::RuleBasedHostResolverProc* rules = host_resolver_->rules();
    rules->AddRuleWithLatency("www.google.com", "127.0.0.1", 50);
    rules->AddRuleWithLatency("gmail.google.com.com", "127.0.0.1", 70);
    rules->AddRuleWithLatency("mail.google.com", "127.0.0.1", 44);
    rules->AddRuleWithLatency("gmail.com", "127.0.0.1", 63);
  }

  void WaitForResolution(DnsMaster* master, const NameList& hosts) {
    HelperTimer* timer = new HelperTimer();
    timer->Start(TimeDelta::FromMilliseconds(100),
                 new WaitForResolutionHelper(master, hosts, timer),
                 &WaitForResolutionHelper::Run);
    MessageLoop::current()->Run();
  }

 private:
  // IMPORTANT: do not move this below |host_resolver_|; the host resolver
  // must not outlive the message loop, otherwise bad things can happen
  // (like posting to a deleted message loop).
  MessageLoop loop_;
  ChromeThread io_thread_;

 protected:
  scoped_refptr<net::MockCachingHostResolver> host_resolver_;

  // Shorthand to access TimeDelta of DnsGlobalInit::kMaxQueueingDelayMs.
  // (It would be a static constant... except style rules preclude that :-/ ).
  const TimeDelta default_max_queueing_delay_;
};

//------------------------------------------------------------------------------

TEST_F(DnsMasterTest, StartupShutdownTest) {
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);
  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, BenefitLookupTest) {
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(
      host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);

  net::HostPortPair goog("www.google.com", 80),
      goog2("gmail.google.com.com", 80),
      goog3("mail.google.com", 80),
      goog4("gmail.com", 80);
  DnsHostInfo goog_info, goog2_info, goog3_info, goog4_info;

  // Simulate getting similar names from a network observer
  goog_info.SetHostname(goog);
  goog2_info.SetHostname(goog2);
  goog3_info.SetHostname(goog3);
  goog4_info.SetHostname(goog4);

  goog_info.SetStartedState();
  goog2_info.SetStartedState();
  goog3_info.SetStartedState();
  goog4_info.SetStartedState();

  goog_info.SetFinishedState(true);
  goog2_info.SetFinishedState(true);
  goog3_info.SetFinishedState(true);
  goog4_info.SetFinishedState(true);

  NameList names;
  names.push_back(goog.host);
  names.push_back(goog2.host);
  names.push_back(goog3.host);
  names.push_back(goog4.host);

  testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));
  EXPECT_TRUE(testing_master->WasFound(goog2));
  EXPECT_TRUE(testing_master->WasFound(goog3));
  EXPECT_TRUE(testing_master->WasFound(goog4));

  // With the mock DNS, each of these should have taken some time, and hence
  // shown a benefit (i.e., prefetch cost more than network access time).

  net::HostPortPair referer;  // Null host.

  // Simulate actual navigation, and acrue the benefit for "helping" the DNS
  // part of the navigation.
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(referer, &goog_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(referer, &goog2_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(referer, &goog3_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(referer, &goog4_info));

  // Benefits can ONLY be reported once (for the first navigation).
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(referer, &goog_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(referer, &goog2_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(referer, &goog3_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(referer, &goog4_info));

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, ShutdownWhenResolutionIsPendingTest) {
  scoped_refptr<net::WaitingHostResolverProc> resolver_proc =
      new net::WaitingHostResolverProc(NULL);
  host_resolver_->Reset(resolver_proc);

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);

  net::HostPortPair localhost("127.0.0.1", 80);
  NameList names;
  names.push_back(localhost.host);

  testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 500);
  MessageLoop::current()->Run();

  EXPECT_FALSE(testing_master->WasFound(localhost));

  testing_master->Shutdown();

  // Clean up after ourselves.
  resolver_proc->Signal();
  MessageLoop::current()->RunAllPending();
}

TEST_F(DnsMasterTest, SingleLookupTest) {
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);

  net::HostPortPair goog("www.google.com", 80);

  NameList names;
  names.push_back(goog.host);

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));

  MessageLoop::current()->RunAllPending();

  EXPECT_GT(testing_master->peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_lookups());

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, ConcurrentLookupTest) {
  host_resolver_->rules()->AddSimulatedFailure("*.notfound");

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);

  net::HostPortPair goog("www.google.com", 80),
      goog2("gmail.google.com.com", 80),
      goog3("mail.google.com", 80),
      goog4("gmail.com", 80);
  net::HostPortPair bad1("bad1.notfound", 80),
      bad2("bad2.notfound", 80);

  NameList names;
  names.push_back(goog.host);
  names.push_back(goog3.host);
  names.push_back(bad1.host);
  names.push_back(goog2.host);
  names.push_back(bad2.host);
  names.push_back(goog4.host);
  names.push_back(goog.host);

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));
  EXPECT_TRUE(testing_master->WasFound(goog3));
  EXPECT_TRUE(testing_master->WasFound(goog2));
  EXPECT_TRUE(testing_master->WasFound(goog4));
  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  EXPECT_GT(testing_master->peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_lookups());

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, MassiveConcurrentLookupTest) {
  host_resolver_->rules()->AddSimulatedFailure("*.notfound");

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(
      host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);

  NameList names;
  for (int i = 0; i < 100; i++)
    names.push_back("host" + IntToString(i) + ".notfound");

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  MessageLoop::current()->RunAllPending();

  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_lookups());

  testing_master->Shutdown();
}

//------------------------------------------------------------------------------
// Functions to help synthesize and test serializations of subresource referrer
// lists.

// Return a motivation_list if we can find one for the given motivating_host (or
// NULL if a match is not found).
static ListValue* FindSerializationMotivation(
    const net::HostPortPair& motivation, const ListValue& referral_list) {
  CHECK_LT(0u, referral_list.GetSize());  // Room for version.
  int format_version = -1;
  CHECK(referral_list.GetInteger(0, &format_version));
  CHECK_EQ(DnsMaster::DNS_REFERRER_VERSION, format_version);
  ListValue* motivation_list(NULL);
  for (size_t i = 1; i < referral_list.GetSize(); ++i) {
    referral_list.GetList(i, &motivation_list);
    std::string existing_host;
    int existing_port;
    EXPECT_TRUE(motivation_list->GetInteger(0, &existing_port));
    EXPECT_TRUE(motivation_list->GetString(1, &existing_host));
    if (motivation.host == existing_host && motivation.port == existing_port)
      return motivation_list;
  }
  return NULL;
}

// Create a new empty serialization list.
static ListValue* NewEmptySerializationList() {
  ListValue* list = new ListValue;
  list->Append(new FundamentalValue(DnsMaster::DNS_REFERRER_VERSION));
  return list;
}

// Add a motivating_host and a subresource_host to a serialized list, using
// this given latency. This is a helper function for quickly building these
// lists.
static void AddToSerializedList(const net::HostPortPair& motivation,
                                const net::HostPortPair& subresource,
                                int latency,
                                double rate,
                                ListValue* referral_list ) {
  // Find the motivation if it is already used.
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           *referral_list);
  if (!motivation_list) {
    // This is the first mention of this motivation, so build a list.
    motivation_list = new ListValue;
    motivation_list->Append(new FundamentalValue(motivation.port));
    motivation_list->Append(new StringValue(motivation.host));
    // Provide empty subresource list.
    motivation_list->Append(new ListValue());

    // ...and make it part of the serialized referral_list.
    referral_list->Append(motivation_list);
  }

  ListValue* subresource_list(NULL);
  // 0 == port; 1 == host; 2 == subresource_list.
  EXPECT_TRUE(motivation_list->GetList(2, &subresource_list));

  // We won't bother to check for the subresource being there already.  Worst
  // case, during deserialization, the latency value we supply plus the
  // existing value(s) will be added to the referrer.

  subresource_list->Append(new FundamentalValue(subresource.port));
  subresource_list->Append(new StringValue(subresource.host));
  subresource_list->Append(new FundamentalValue(latency));
  subresource_list->Append(new FundamentalValue(rate));
}

static const int kLatencyNotFound = -1;

// For a given motivation, and subresource, find what latency is currently
// listed.  This assume a well formed serialization, which has at most one such
// entry for any pair of names.  If no such pair is found, then return false.
// Data is written into rate and latency arguments.
static bool GetDataFromSerialization(const net::HostPortPair& motivation,
                                     const net::HostPortPair& subresource,
                                     const ListValue& referral_list,
                                     double* rate,
                                     int* latency) {
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           referral_list);
  if (!motivation_list)
    return false;
  ListValue* subresource_list;
  EXPECT_TRUE(motivation_list->GetList(2, &subresource_list));
  for (size_t i = 0; i < subresource_list->GetSize();) {
    std::string host;
    int port;
    EXPECT_TRUE(subresource_list->GetInteger(i++, &port));
    EXPECT_TRUE(subresource_list->GetString(i++, &host));
    EXPECT_TRUE(subresource_list->GetInteger(i++, latency));
    EXPECT_TRUE(subresource_list->GetReal(i++, rate));
    if (subresource.host == host  && subresource.port == port) {
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------

// Make sure nil referral lists really have no entries, and no latency listed.
TEST_F(DnsMasterTest, ReferrerSerializationNilTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);
  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());
  master->SerializeReferrers(referral_list.get());
  EXPECT_EQ(1U, referral_list->GetSize());
  EXPECT_FALSE(GetDataFromSerialization(
      net::HostPortPair("a.com", 79), net::HostPortPair("b.com", 78),
      *referral_list.get(), NULL, NULL));

  master->Shutdown();
}

// Make sure that when a serialization list includes a value, that it can be
// deserialized into the database, and can be extracted back out via
// serialization without being changed.
TEST_F(DnsMasterTest, ReferrerSerializationSingleReferrerTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);
  const net::HostPortPair motivation_hostport("www.google.com", 91);
  const net::HostPortPair subresource_hostport("icons.google.com", 90);
  const int kLatency = 3;
  const double kRate = 23.4;
  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());

  AddToSerializedList(motivation_hostport, subresource_hostport,
      kLatency, kRate, referral_list.get());

  master->DeserializeReferrers(*referral_list.get());

  ListValue recovered_referral_list;
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  int latency;
  double rate;
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, subresource_hostport, recovered_referral_list, &rate,
      &latency));
  EXPECT_EQ(rate, kRate);
  EXPECT_EQ(latency, kLatency);

  master->Shutdown();
}

// Make sure the Trim() functionality works as expected.
TEST_F(DnsMasterTest, ReferrerSerializationTrimTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(host_resolver_,
      default_max_queueing_delay_,
      DnsGlobalInit::kMaxPrefetchConcurrentLookups,
      false);
  net::HostPortPair motivation_hostport("www.google.com", 110);

  net::HostPortPair icon_subresource_hostport("icons.google.com", 111);
  const int kLatencyIcon = 10;
  const double kRateIcon = 0.;  // User low rate, so latency will dominate.
  net::HostPortPair img_subresource_hostport("img.google.com", 118);
  const int kLatencyImg = 3;
  const double kRateImg = 0.;

  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());
  AddToSerializedList(
      motivation_hostport, icon_subresource_hostport,
      kLatencyIcon, kRateIcon, referral_list.get());
  AddToSerializedList(
      motivation_hostport, img_subresource_hostport,
      kLatencyImg, kRateImg, referral_list.get());

  master->DeserializeReferrers(*referral_list.get());

  ListValue recovered_referral_list;
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  int latency;
  double rate;
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, icon_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyIcon);
  EXPECT_EQ(rate, kRateIcon);

  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, img_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyImg);
  EXPECT_EQ(rate, kRateImg);

  // Each time we Trim, the latency figures should reduce by a factor of two,
  // until they both are 0, an then a trim will delete the whole entry.
  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, icon_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyIcon / 2);
  EXPECT_EQ(rate, kRateIcon);

  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, img_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyImg / 2);
  EXPECT_EQ(rate, kRateImg);

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, icon_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyIcon / 4);
  EXPECT_EQ(rate, kRateIcon);
  // Img is down to zero, but we don't delete it yet.
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, img_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(kLatencyImg / 4, 0);
  EXPECT_EQ(latency, kLatencyImg / 4);
  EXPECT_EQ(rate, kRateImg);

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, icon_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(latency, kLatencyIcon / 8);
  EXPECT_EQ(rate, kRateIcon);

  // Img is down to zero, but we don't delete it yet.
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_hostport, img_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_EQ(kLatencyImg / 8, 0);
  EXPECT_EQ(latency, kLatencyImg / 8);
  EXPECT_EQ(rate, kRateImg);

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  // Icon is also trimmed away, so entire set gets discarded.
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_FALSE(GetDataFromSerialization(
      motivation_hostport, icon_subresource_hostport, recovered_referral_list,
      &rate, &latency));
  EXPECT_FALSE(GetDataFromSerialization(
      motivation_hostport, img_subresource_hostport, recovered_referral_list,
      &rate, &latency));

  master->Shutdown();
}


TEST_F(DnsMasterTest, PriorityQueuePushPopTest) {
  DnsMaster::HostNameQueue queue;

  net::HostPortPair first("first", 80), second("second", 90);

  // First check high priority queue FIFO functionality.
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push(first, DnsHostInfo::LEARNED_REFERAL_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push(second, DnsHostInfo::MOUSE_OVER_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop().ToString(), first.ToString());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop().ToString(), second.ToString());
  EXPECT_TRUE(queue.IsEmpty());

  // Then check low priority queue FIFO functionality.
  queue.Push(first, DnsHostInfo::PAGE_SCAN_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push(second, DnsHostInfo::OMNIBOX_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop().ToString(), first.ToString());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop().ToString(), second.ToString());
  EXPECT_TRUE(queue.IsEmpty());
}

TEST_F(DnsMasterTest, PriorityQueueReorderTest) {
  DnsMaster::HostNameQueue queue;

  // Push all the low priority items.
  net::HostPortPair low1("low1", 80),
      low2("low2", 80),
      low3("low3", 443),
      low4("low4", 80),
      low5("low5", 80),
      hi1("hi1", 80),
      hi2("hi2", 80),
      hi3("hi3", 80);

  EXPECT_TRUE(queue.IsEmpty());
  queue.Push(low1, DnsHostInfo::PAGE_SCAN_MOTIVATED);
  queue.Push(low2, DnsHostInfo::UNIT_TEST_MOTIVATED);
  queue.Push(low3, DnsHostInfo::LINKED_MAX_MOTIVATED);
  queue.Push(low4, DnsHostInfo::OMNIBOX_MOTIVATED);
  queue.Push(low5, DnsHostInfo::STARTUP_LIST_MOTIVATED);
  queue.Push(low4, DnsHostInfo::OMNIBOX_MOTIVATED);

  // Push all the high prority items
  queue.Push(hi1, DnsHostInfo::LEARNED_REFERAL_MOTIVATED);
  queue.Push(hi2, DnsHostInfo::STATIC_REFERAL_MOTIVATED);
  queue.Push(hi3, DnsHostInfo::MOUSE_OVER_MOTIVATED);

  // Check that high priority stuff comes out first, and in FIFO order.
  EXPECT_EQ(queue.Pop().ToString(), hi1.ToString());
  EXPECT_EQ(queue.Pop().ToString(), hi2.ToString());
  EXPECT_EQ(queue.Pop().ToString(), hi3.ToString());

  // ...and then low priority strings.
  EXPECT_EQ(queue.Pop().ToString(), low1.ToString());
  EXPECT_EQ(queue.Pop().ToString(), low2.ToString());
  EXPECT_EQ(queue.Pop().ToString(), low3.ToString());
  EXPECT_EQ(queue.Pop().ToString(), low4.ToString());
  EXPECT_EQ(queue.Pop().ToString(), low5.ToString());
  EXPECT_EQ(queue.Pop().ToString(), low4.ToString());

  EXPECT_TRUE(queue.IsEmpty());
}

}  // namespace chrome_browser_net
