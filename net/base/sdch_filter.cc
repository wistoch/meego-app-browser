// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <ctype.h>
#include <algorithm>

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/time.h"
#include "net/base/sdch_filter.h"
#include "net/base/sdch_manager.h"

#include "sdch/open-vcdiff/src/google/vcdecoder.h"

SdchFilter::SdchFilter(const FilterContext& filter_context)
    : Filter(filter_context),
      decoding_status_(DECODING_UNINITIALIZED),
      vcdiff_streaming_decoder_(NULL),
      dictionary_hash_(),
      dictionary_hash_is_plausible_(false),
      dictionary_(NULL),
      dest_buffer_excess_(),
      dest_buffer_excess_index_(0),
      source_bytes_(0),
      output_bytes_(0),
      possible_pass_through_(false),
      connect_time_(filter_context.GetRequestTime()),
      was_cached_(filter_context.IsCachedContent()) {
  bool success = filter_context.GetMimeType(&mime_type_);
  DCHECK(success);
  success = filter_context.GetURL(&url_);
  DCHECK(success);
}

SdchFilter::~SdchFilter() {
  // All code here is for gathering stats, and can be removed when SDCH is
  // considered stable.

  static int filter_use_count = 0;
  ++filter_use_count;
  if (META_REFRESH_RECOVERY == decoding_status_) {
    UMA_HISTOGRAM_COUNTS("Sdch.FilterUseBeforeDisabling", filter_use_count);
  }

  if (vcdiff_streaming_decoder_.get()) {
    if (!vcdiff_streaming_decoder_->FinishDecoding()) {
      decoding_status_ = DECODING_ERROR;
      SdchManager::SdchErrorRecovery(SdchManager::INCOMPLETE_SDCH_CONTENT);
      // Make it possible for the user to hit reload, and get non-sdch content.
      // Note this will "wear off" quickly enough, and is just meant to assure
      // in some rare case that the user is not stuck.
      SdchManager::BlacklistDomain(url_);
      UMA_HISTOGRAM_COUNTS("Sdch.PartialBytesIn",
           static_cast<int>(filter_context().GetByteReadCount()));
      UMA_HISTOGRAM_COUNTS("Sdch.PartialVcdiffIn", source_bytes_);
      UMA_HISTOGRAM_COUNTS("Sdch.PartialVcdiffOut", output_bytes_);
    }
  }

  if (!dest_buffer_excess_.empty()) {
    // Filter chaining error, or premature teardown.
    SdchManager::SdchErrorRecovery(SdchManager::UNFLUSHED_CONTENT);
    UMA_HISTOGRAM_COUNTS("Sdch.UnflushedBytesIn",
         static_cast<int>(filter_context().GetByteReadCount()));
    UMA_HISTOGRAM_COUNTS("Sdch.UnflushedBufferSize",
                         dest_buffer_excess_.size());
    UMA_HISTOGRAM_COUNTS("Sdch.UnflushedVcdiffIn", source_bytes_);
    UMA_HISTOGRAM_COUNTS("Sdch.UnflushedVcdiffOut", output_bytes_);
  }

  if (was_cached_) {
    // Not a real error, but it is useful to have this tally.
    // TODO(jar): Remove this stat after SDCH stability is validated.
    SdchManager::SdchErrorRecovery(SdchManager::CACHE_DECODED);
    return;  // We don't need timing stats, and we aready got ratios.
  }

  if (base::Time() == connect_time_ || read_times_.empty()) {
    // Not a real error, but it is useful to have this tally.
    // TODO(jar): Remove this stat after SDCH stability is validated.
    SdchManager::SdchErrorRecovery(SdchManager::MISSING_TIME_STATS);
    UMA_HISTOGRAM_COUNTS("Sdch.MissingTimeBytesIn",
         static_cast<int>(filter_context().GetByteReadCount()));
    UMA_HISTOGRAM_COUNTS("Sdch.MissingTimeVcdiffIn", source_bytes_);
    return;
  }

  base::TimeDelta duration = read_times_.back() - connect_time_;
  // We clip our logging at 10 minutes to prevent anamolous data from being
  // considered (per suggestion from Jake Brutlag).
  if (10 < duration.InMinutes()) {
    SdchManager::SdchErrorRecovery(SdchManager::OVER_10_MINUTES);
    return;
  }

  switch (decoding_status_) {
    case DECODING_IN_PROGRESS: {
      UMA_HISTOGRAM_PERCENTAGE("Sdch.Network_Decode_Ratio_a", static_cast<int>(
          (filter_context().GetByteReadCount() * 100) / output_bytes_));
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Decode_Latency_F_a", duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Decode_1st_To_Last_a",
                                  read_times_.back() - read_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      if (read_times_.size() > 4) {
        UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Decode_3rd_To_4th_a",
                                    read_times_[3] - read_times_[2],
                                    base::TimeDelta::FromMilliseconds(10),
                                    base::TimeDelta::FromSeconds(3), 100);
        UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Decode_4th_To_5th_a",
                                    read_times_[4] - read_times_[3],
                                    base::TimeDelta::FromMilliseconds(10),
                                    base::TimeDelta::FromSeconds(3), 100);
      }
      UMA_HISTOGRAM_COUNTS_100("Sdch.Network_Decode_Packets_a",
                               read_times_.size());
      UMA_HISTOGRAM_COUNTS("Sdch.Network_Decode_Bytes_Processed_a",
          static_cast<int>(filter_context().GetByteReadCount()));
      UMA_HISTOGRAM_COUNTS("Sdch.Network_Decode_Bytes_VcdiffOut_a",
                           output_bytes_);
      return;
    }
    case PASS_THROUGH: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Pass-through_Latency_F_a",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Pass-through_1st_To_Last_a",
                                  read_times_.back() - read_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      if (read_times_.size() > 4) {
        UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Pass-through_3rd_To_4th_a",
                                    read_times_[3] - read_times_[2],
                                    base::TimeDelta::FromMilliseconds(10),
                                    base::TimeDelta::FromSeconds(3), 100);
        UMA_HISTOGRAM_CLIPPED_TIMES("Sdch.Network_Pass-through_4th_To_5th_a",
                                    read_times_[4] - read_times_[3],
                                    base::TimeDelta::FromMilliseconds(10),
                                    base::TimeDelta::FromSeconds(3), 100);
      }
      UMA_HISTOGRAM_COUNTS_100("Sdch.Network_Pass-through_Packets_a",
                               read_times_.size());
      return;
    }
    case DECODING_UNINITIALIZED: {
      SdchManager::SdchErrorRecovery(SdchManager::UNINITIALIZED);
      return;
    }
    case WAITING_FOR_DICTIONARY_SELECTION: {
      SdchManager::SdchErrorRecovery(SdchManager::PRIOR_TO_DICTIONARY);
      return;
    }
    case DECODING_ERROR: {
      SdchManager::SdchErrorRecovery(SdchManager::DECODE_ERROR);
      return;
    }
    case META_REFRESH_RECOVERY: {
      // Already accounted for when set.
      return;
    }
  }  // end of switch.
}

void SdchFilter::UpdateReadTimes() {
  if (!next_stream_data_ || (stream_data_len_ == 0)) {
    // Don't update when we're called to just flush out our internal buffers.
    return;
  }
  const int64 bytes_read_so_far = filter_context().GetByteReadCount();
  if (bytes_read_so_far <= 0)
    return;
  const size_t kTypicalPacketSize = 1430;
  // For ByteReadCount up to 1430, we have 1 packet.  Up to 2860, 2 packets etc.
  if (bytes_read_so_far > 100 * kTypicalPacketSize)
    return;  // Let's not stress the array size.
  const size_t bytes = static_cast<size_t>(bytes_read_so_far);
  const size_t probable_packet_number = 1 + (bytes - 1) / kTypicalPacketSize;
  base::Time now = base::Time::Now();
  while (probable_packet_number > read_times_.size())
    read_times_.push_back(now);
}

bool SdchFilter::InitDecoding(Filter::FilterType filter_type) {
  if (decoding_status_ != DECODING_UNINITIALIZED)
    return false;

  // Handle case  where sdch filter is guessed, but not required.
  if (FILTER_TYPE_SDCH_POSSIBLE == filter_type)
    possible_pass_through_ = true;

  // Initialize decoder only after we have a dictionary in hand.
  decoding_status_ = WAITING_FOR_DICTIONARY_SELECTION;
  return true;
}

static const char* kDecompressionErrorHtml =
  "<head><META HTTP-EQUIV=\"Refresh\" CONTENT=\"0\"></head>"
  "<div style=\"position:fixed;top:0;left:0;width:100%;border-width:thin;"
  "border-color:black;border-style:solid;text-align:left;font-family:arial;"
  "font-size:10pt;foreground-color:black;background-color:white\">"
  "An error occurred. This page will be reloaded shortly. "
  "Or press the \"reload\" button now to reload it immediately."
  "</div>";


Filter::FilterStatus SdchFilter::ReadFilteredData(char* dest_buffer,
                                                  int* dest_len) {
  int available_space = *dest_len;
  *dest_len = 0;  // Nothing output yet.

  if (!dest_buffer || available_space <= 0)
    return FILTER_ERROR;

  UpdateReadTimes();

  if (WAITING_FOR_DICTIONARY_SELECTION == decoding_status_) {
    FilterStatus status = InitializeDictionary();
    if (FILTER_NEED_MORE_DATA == status)
      return FILTER_NEED_MORE_DATA;
    if (FILTER_ERROR == status) {
      DCHECK(DECODING_ERROR == decoding_status_);
      DCHECK(0 == dest_buffer_excess_index_);
      DCHECK(dest_buffer_excess_.empty());
      if (possible_pass_through_) {
        // We added the sdch coding tag, and it should not have been added.
        // This can happen in server experiments, where the server decides
        // not to use sdch, even though there is a dictionary.  To be
        // conservative, we locally added the tentative sdch (fearing that a
        // proxy stripped it!) and we must now recant (pass through).
        SdchManager::SdchErrorRecovery(SdchManager::DISCARD_TENTATIVE_SDCH);
        decoding_status_ = PASS_THROUGH;
        dest_buffer_excess_ = dictionary_hash_;  // Send what we scanned.
      } else if (!dictionary_hash_is_plausible_) {
        // One of the first 9 bytes precluded consideration as a hash.
        // This can't be an SDCH payload, even though the server said it was.
        // This is a major error, as the server or proxy tagged this SDCH even
        // though it is not!
        // The good news is that error recovery is clear...
        SdchManager::SdchErrorRecovery(SdchManager::PASSING_THROUGH_NON_SDCH);
        decoding_status_ = PASS_THROUGH;
        dest_buffer_excess_ = dictionary_hash_;  // Send what we scanned.
      } else {
        // We don't have the dictionary that was demanded.
        // With very low probability, random garbage data looked like a
        // dictionary specifier (8 ASCII characters followed by a null), but
        // that is sufficiently unlikely that we ignore it.
        if (std::string::npos == mime_type_.find_first_of("text/html")) {
          SdchManager::BlacklistDomainForever(url_);
          if (was_cached_)
            SdchManager::SdchErrorRecovery(
                SdchManager::CACHED_META_REFRESH_UNSUPPORTED);
          else
            SdchManager::SdchErrorRecovery(
                SdchManager::META_REFRESH_UNSUPPORTED);
          return FILTER_ERROR;
        }
        // HTML content means we can issue a meta-refresh, and get the content
        // again, perhaps without SDCH (to be safe).
        if (was_cached_) {
          // Cached content is probably a startup tab, so we'll just get fresh
          // content and try again, without disabling sdch.
          SdchManager::SdchErrorRecovery(
              SdchManager::META_REFRESH_CACHED_RECOVERY);
        } else {
          // Since it wasn't in the cache, we definately need at least some
          // period of blacklisting to get the correct content.
          SdchManager::BlacklistDomain(url_);
          SdchManager::SdchErrorRecovery(SdchManager::META_REFRESH_RECOVERY);
        }
        decoding_status_ = META_REFRESH_RECOVERY;
        // Issue a meta redirect with SDCH disabled.
        dest_buffer_excess_ = kDecompressionErrorHtml;
      }
    } else {
      DCHECK(DECODING_IN_PROGRESS == decoding_status_);
    }
  }

  int amount = OutputBufferExcess(dest_buffer, available_space);
  *dest_len += amount;
  dest_buffer += amount;
  available_space -= amount;
  DCHECK(available_space >= 0);

  if (available_space <= 0)
    return FILTER_OK;
  DCHECK(dest_buffer_excess_.empty());
  DCHECK(0 == dest_buffer_excess_index_);

  if (decoding_status_ != DECODING_IN_PROGRESS) {
    if (META_REFRESH_RECOVERY == decoding_status_) {
      // Absorb all input data.  We've already output page reload HTML.
      next_stream_data_ = NULL;
      stream_data_len_ = 0;
      return FILTER_NEED_MORE_DATA;
    }
    if (PASS_THROUGH == decoding_status_) {
      // We must pass in available_space, but it will be changed to bytes_used.
      FilterStatus result = CopyOut(dest_buffer, &available_space);
      // Accumulate the returned count of bytes_used (a.k.a., available_space).
      *dest_len += available_space;
      return result;
    }
    DCHECK(false);
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }

  if (!next_stream_data_ || stream_data_len_ <= 0)
    return FILTER_NEED_MORE_DATA;

  bool ret = vcdiff_streaming_decoder_->DecodeChunk(
    next_stream_data_, stream_data_len_, &dest_buffer_excess_);
  // Assume all data was used in decoding.
  next_stream_data_ = NULL;
  source_bytes_ += stream_data_len_;
  stream_data_len_ = 0;
  output_bytes_ += dest_buffer_excess_.size();
  if (!ret) {
    vcdiff_streaming_decoder_.reset(NULL);  // Don't call it again.
    decoding_status_ = DECODING_ERROR;
    SdchManager::SdchErrorRecovery(SdchManager::DECODE_BODY_ERROR);
    return FILTER_ERROR;
  }

  amount = OutputBufferExcess(dest_buffer, available_space);
  *dest_len += amount;
  dest_buffer += amount;
  available_space -= amount;
  if (0 == available_space && !dest_buffer_excess_.empty())
      return FILTER_OK;
  return FILTER_NEED_MORE_DATA;
}

Filter::FilterStatus SdchFilter::InitializeDictionary() {
  const size_t kServerIdLength = 9;  // Dictionary hash plus null from server.
  size_t bytes_needed = kServerIdLength - dictionary_hash_.size();
  DCHECK(bytes_needed > 0);
  if (!next_stream_data_)
    return FILTER_NEED_MORE_DATA;
  if (static_cast<size_t>(stream_data_len_) < bytes_needed) {
    dictionary_hash_.append(next_stream_data_, stream_data_len_);
    next_stream_data_ = NULL;
    stream_data_len_ = 0;
    return FILTER_NEED_MORE_DATA;
  }
  dictionary_hash_.append(next_stream_data_, bytes_needed);
  DCHECK(kServerIdLength == dictionary_hash_.size());
  stream_data_len_ -= bytes_needed;
  DCHECK(0 <= stream_data_len_);
  if (stream_data_len_ > 0)
    next_stream_data_ += bytes_needed;
  else
    next_stream_data_ = NULL;

  DCHECK(!dictionary_.get());
  dictionary_hash_is_plausible_ = true;  // Assume plausible, but check.

  SdchManager::Dictionary* dictionary = NULL;
  if ('\0' == dictionary_hash_[kServerIdLength - 1])
    SdchManager::Global()->GetVcdiffDictionary(std::string(dictionary_hash_, 0,
                                                           kServerIdLength - 1),
                                               url_, &dictionary);
  else
    dictionary_hash_is_plausible_ = false;

  if (!dictionary) {
    DCHECK(dictionary_hash_.size() == kServerIdLength);
    // Since dictionary was not found, check to see if hash was even plausible.
    for (size_t i = 0; i < kServerIdLength - 1; ++i) {
      char base64_char = dictionary_hash_[i];
      if (!isalnum(base64_char) && '-' != base64_char && '_' != base64_char) {
        dictionary_hash_is_plausible_ = false;
        break;
      }
    }
    if (dictionary_hash_is_plausible_)
      SdchManager::SdchErrorRecovery(SdchManager::DICTIONARY_HASH_NOT_FOUND);
    else
      SdchManager::SdchErrorRecovery(SdchManager::DICTIONARY_HASH_MALFORMED);
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }
  dictionary_ = dictionary;
  vcdiff_streaming_decoder_.reset(new open_vcdiff::VCDiffStreamingDecoder);
  vcdiff_streaming_decoder_->StartDecoding(dictionary_->text().data(),
                                           dictionary_->text().size());
  decoding_status_ = DECODING_IN_PROGRESS;
  return FILTER_OK;
}

int SdchFilter::OutputBufferExcess(char* const dest_buffer,
                                   size_t available_space) {
  if (dest_buffer_excess_.empty())
    return 0;
  DCHECK(dest_buffer_excess_.size() > dest_buffer_excess_index_);
  size_t amount = std::min(available_space,
      dest_buffer_excess_.size() - dest_buffer_excess_index_);
  memcpy(dest_buffer, dest_buffer_excess_.data() + dest_buffer_excess_index_,
         amount);
  dest_buffer_excess_index_ += amount;
  if (dest_buffer_excess_.size() <= dest_buffer_excess_index_) {
    DCHECK(dest_buffer_excess_.size() == dest_buffer_excess_index_);
    dest_buffer_excess_.clear();
    dest_buffer_excess_index_ = 0;
  }
  return amount;
}
