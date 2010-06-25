// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active profile in Chrome.
//
// Each download is represented by a DownloadItem, and all DownloadItems
// are owned by the DownloadManager which maintains a global list of all
// downloads. DownloadItems are created when a user initiates a download,
// and exist for the duration of the browser life time.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadItem::Observer:
//     - allows observers to receive notifications about one download from start
//       to completion
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/shell_dialogs.h"

class DownloadFileManager;
class DownloadItemView;
class DownloadManager;
class GURL;
class PrefService;
class Profile;
class ResourceDispatcherHost;
class URLRequestContextGetter;
class TabContents;
struct DownloadSaveInfo;

namespace base {
class Thread;
}

// DownloadItem ----------------------------------------------------------------

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
class DownloadItem {
 public:
  enum DownloadState {
    IN_PROGRESS,
    COMPLETE,
    CANCELLED,
    REMOVING
  };

  enum SafetyState {
    SAFE = 0,
    DANGEROUS,
    DANGEROUS_BUT_VALIDATED  // Dangerous but the user confirmed the download.
  };

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) = 0;

    // Called when a downloaded file has been completed.
    virtual void OnDownloadFileCompleted(DownloadItem* download) = 0;

    // Called when a downloaded file has been opened.
    virtual void OnDownloadOpened(DownloadItem* download) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Constructing from persistent store:
  explicit DownloadItem(const DownloadCreateInfo& info);

  // Constructing from user action:
  DownloadItem(int32 download_id,
               const FilePath& path,
               int path_uniquifier,
               const GURL& url,
               const GURL& referrer_url,
               const std::string& mime_type,
               const std::string& original_mime_type,
               const FilePath& original_name,
               const base::Time start_time,
               int64 download_size,
               int render_process_id,
               int request_id,
               bool is_dangerous,
               bool save_as,
               bool is_otr,
               bool is_extension_install,
               bool is_temporary);

  ~DownloadItem();

  void Init(bool start_timer);

  // Public API

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies our observers periodically.
  void UpdateObservers();

  // Notifies our observers the downloaded file has been completed.
  void NotifyObserversDownloadFileCompleted();

  // Notifies our observers the downloaded file has been opened.
  void NotifyObserversDownloadOpened();

  // Received a new chunk of data
  void Update(int64 bytes_so_far);

  // Cancel the download operation. We need to distinguish between cancels at
  // exit (DownloadManager destructor) from user interface initiated cancels
  // because at exit, the history system may not exist, and any updates to it
  // require AddRef'ing the DownloadManager in the destructor which results in
  // a DCHECK failure. Set 'update_history' to false when canceling from at
  // exit to prevent this crash. This may result in a difference between the
  // downloaded file's size on disk, and what the history system's last record
  // of it is. At worst, we'll end up re-downloading a small portion of the file
  // when resuming a download (assuming the server supports byte ranges).
  void Cancel(bool update_history);

  // Download operation completed.
  void Finished(int64 size);

  // The user wants to remove the download from the views and history. If
  // |delete_file| is true, the file is deleted on the disk.
  void Remove(bool delete_file);

  // Start/stop sending periodic updates to our observers
  void StartProgressTimer();
  void StopProgressTimer();

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  bool TimeRemaining(base::TimeDelta* remaining) const;

  // Simple speed estimate in bytes/s
  int64 CurrentSpeed() const;

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete() const;

  // Update the download's path, the actual file is renamed on the download
  // thread.
  void Rename(const FilePath& full_path);

  // Allow the user to temporarily pause a download or resume a paused download.
  void TogglePause();

  // Accessors
  DownloadState state() const { return state_; }
  FilePath file_name() const { return file_name_; }
  void set_file_name(const FilePath& name) { file_name_ = name; }
  FilePath full_path() const { return full_path_; }
  void set_full_path(const FilePath& path) { full_path_ = path; }
  int path_uniquifier() const { return path_uniquifier_; }
  void set_path_uniquifier(int uniquifier) { path_uniquifier_ = uniquifier; }
  GURL url() const { return url_; }
  GURL referrer_url() const { return referrer_url_; }
  std::string mime_type() const { return mime_type_; }
  std::string original_mime_type() const { return original_mime_type_; }
  int64 total_bytes() const { return total_bytes_; }
  void set_total_bytes(int64 total_bytes) { total_bytes_ = total_bytes; }
  int64 received_bytes() const { return received_bytes_; }
  int32 id() const { return id_; }
  base::Time start_time() const { return start_time_; }
  void set_db_handle(int64 handle) { db_handle_ = handle; }
  int64 db_handle() const { return db_handle_; }
  DownloadManager* manager() const { return manager_; }
  void set_manager(DownloadManager* manager) { manager_ = manager; }
  bool is_paused() const { return is_paused_; }
  void set_is_paused(bool pause) { is_paused_ = pause; }
  bool open_when_complete() const { return open_when_complete_; }
  void set_open_when_complete(bool open) { open_when_complete_ = open; }
  int render_process_id() const { return render_process_id_; }
  int request_id() const { return request_id_; }
  SafetyState safety_state() const { return safety_state_; }
  void set_safety_state(SafetyState safety_state) {
    safety_state_ = safety_state;
  }
  bool auto_opened() { return auto_opened_; }
  void set_auto_opened(bool auto_opened) { auto_opened_ = auto_opened; }
  FilePath original_name() const { return original_name_; }
  void set_original_name(const FilePath& name) { original_name_ = name; }
  bool save_as() const { return save_as_; }
  bool is_otr() const { return is_otr_; }
  bool is_extension_install() const { return is_extension_install_; }
  bool name_finalized() const { return name_finalized_; }
  void set_name_finalized(bool name_finalized) {
    name_finalized_ = name_finalized;
  }
  bool is_temporary() const { return is_temporary_; }
  void set_is_temporary(bool is_temporary) { is_temporary_ = is_temporary; }

  // Returns the file-name that should be reported to the user, which is
  // file_name_ for safe downloads and original_name_ for dangerous ones with
  // the uniquifier number.
  FilePath GetFileName() const;

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Request ID assigned by the ResourceDispatcherHost.
  int32 id_;

  // Full path to the downloaded file
  FilePath full_path_;

  // A number that should be appended to the path to make it unique, or 0 if the
  // path should be used as is.
  int path_uniquifier_;

  // Short display version of the file
  FilePath file_name_;

  // The URL from whence we came.
  GURL url_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

  // The mimetype of the download
  std::string mime_type_;

  // The value of the content type header received when downloading
  // this item.  |mime_type_| may be different because of type sniffing.
  std::string original_mime_type_;

  // Total bytes expected
  int64 total_bytes_;

  // Current received bytes
  int64 received_bytes_;

  // Start time for calculating remaining time
  base::TimeTicks start_tick_;

  // The current state of this download
  DownloadState state_;

  // The views of this item in the download shelf and download tab
  ObserverList<Observer> observers_;

  // Time the download was started
  base::Time start_time_;

  // Our persistent store handle
  int64 db_handle_;

  // Timer for regularly updating our observers
  base::RepeatingTimer<DownloadItem> update_timer_;

  // Our owning object
  DownloadManager* manager_;

  // In progress downloads may be paused by the user, we note it here
  bool is_paused_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // Whether the download is considered potentially safe or dangerous
  // (executable files are typically considered dangerous).
  SafetyState safety_state_;

  // Whether the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_;

  // Dangerous download are given temporary names until the user approves them.
  // This stores their original name.
  FilePath original_name_;

  // For canceling or pausing requests.
  int render_process_id_;
  int request_id_;

  // True if the item was downloaded as a result of 'save as...'
  bool save_as_;

  // True if the download was initiated in an incognito window.
  bool is_otr_;

  // True if the item was downloaded for an extension installation.
  bool is_extension_install_;

  // True if the filename is finalized.
  bool name_finalized_;

  // True if the item was downloaded temporarily.
  bool is_temporary_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItem);
};


// DownloadManager -------------------------------------------------------------

// Browser's download manager: manages all downloads and destination view.
class DownloadManager : public base::RefCountedThreadSafe<DownloadManager>,
                        public SelectFileDialog::Listener {
  // For testing.
  friend class DownloadManagerTest;

 public:
  DownloadManager();

  static void RegisterUserPrefs(PrefService* prefs);

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class Observer {
   public:
    // New or deleted download, observers should query us for the current set
    // of downloads.
    virtual void ModelChanged() = 0;

    // A callback once the DownloadManager has retrieved the requested set of
    // downloads. The DownloadManagerObserver must copy the vector, but does not
    // own the individual DownloadItems, when this call is made.
    virtual void SetDownloads(std::vector<DownloadItem*>& downloads) = 0;

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown() {}

   protected:
    virtual ~Observer() {}
  };

  // Public API

  // If this download manager has an incognito profile, find all incognito
  // downloads and pass them along to the parent profile's download manager
  // via DoGetDownloads. Otherwise, just call DoGetDownloads().
  void GetDownloads(Observer* observer,
                    const std::wstring& search_text);

  // Begin a search for all downloads matching 'search_text'. If 'search_text'
  // is empty, return all known downloads. The results are returned in the
  // 'SetDownloads' observer callback.
  void DoGetDownloads(Observer* observer,
                      const std::wstring& search_text,
                      std::vector<DownloadItem*>& otr_downloads);

  // Return all temporary downloads that reside in the specified directory.
  void GetTemporaryDownloads(Observer* observer,
                             const FilePath& dir_path);

  // Return all non-temporary downloads in the specified directory that are
  // are in progress or have finished.
  void GetAllDownloads(Observer* observer, const FilePath& dir_path);

  // Return all non-temporary downloads in the specified directory that are
  // either in-progress or finished but still waiting for user confirmation.
  void GetCurrentDownloads(Observer* observer, const FilePath& dir_path);

  // Returns true if initialized properly.
  bool Init(Profile* profile);

  // Schedule a query of the history service to retrieve all downloads.
  void QueryHistoryForDownloads();

  // Cleans up IN_PROGRESS history entries as these entries are corrupt because
  // of the sudden exit. Changes them to CANCELED. Executed only when called
  // first time, subsequent calls a no op.
  void CleanUpInProgressHistoryEntries();

  // Notifications sent from the download thread to the UI thread
  void StartDownload(DownloadCreateInfo* info);
  void UpdateDownload(int32 download_id, int64 size);
  void DownloadFinished(int32 download_id, int64 size);

  // Called from a view when a user clicks a UI button or link.
  void DownloadCancelled(int32 download_id);
  void PauseDownload(int32 download_id, bool pause);
  void RemoveDownload(int64 download_handle);

  // Called when the download is renamed to its final name.
  void DownloadRenamedToFinalName(int download_id, const FilePath& full_path);

  // Remove downloads after remove_begin (inclusive) and before remove_end
  // (exclusive). You may pass in null Time values to do an unbounded delete
  // in either direction.
  int RemoveDownloadsBetween(const base::Time remove_begin,
                             const base::Time remove_end);

  // Remove downloads will delete all downloads that have a timestamp that is
  // the same or more recent than |remove_begin|. The number of downloads
  // deleted is returned back to the caller.
  int RemoveDownloads(const base::Time remove_begin);

  // Remove all downloads will delete all downloads. The number of downloads
  // deleted is returned back to the caller.
  int RemoveAllDownloads();

  // Download the object at the URL. Used in cases such as "Save Link As..."
  void DownloadUrl(const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   TabContents* tab_contents);

  // Download the object at the URL and save it to the specified path. The
  // download is treated as the temporary download and thus will not appear
  // in the download history. Used in cases such as drag and drop.
  void DownloadUrlToFile(const GURL& url,
                         const GURL& referrer,
                         const std::string& referrer_encoding,
                         const DownloadSaveInfo& save_info,
                         TabContents* tab_contents);

  // Allow objects to observe the download creation process.
  void AddObserver(Observer* observer);

  // Remove a download observer from ourself.
  void RemoveObserver(Observer* observer);

  // Methods called on completion of a query sent to the history system.
  void OnQueryDownloadEntriesComplete(
      std::vector<DownloadCreateInfo>* entries);
  void OnCreateDownloadEntryComplete(DownloadCreateInfo info, int64 db_handle);
  void OnSearchComplete(HistoryService::Handle handle,
                        std::vector<int64>* results);

  // Display a new download in the appropriate browser UI.
  void ShowDownloadInBrowser(const DownloadCreateInfo& info,
                             DownloadItem* download);

  // Opens a download. For Chrome extensions call
  // ExtensionsServices::InstallExtension, for everything else call
  // OpenDownloadInShell.
  void OpenDownload(const DownloadItem* download,
                    gfx::NativeView parent_window);

  // Show a download via the Windows shell.
  void ShowDownloadInShell(const DownloadItem* download);

  // The number of in progress (including paused) downloads.
  int in_progress_count() const {
    return static_cast<int>(in_progress_.size());
  }

  FilePath download_path() { return *download_path_; }

  // Clears the last download path, used to initialize "save as" dialogs.
  void ClearLastDownloadPath();

  // Registers this file extension for automatic opening upon download
  // completion if 'open' is true, or prevents the extension from automatic
  // opening if 'open' is false.
  void OpenFilesBasedOnExtension(const FilePath& path, bool open);

  // Tests if a file type should be opened automatically.
  bool ShouldOpenFileBasedOnExtension(const FilePath& path) const;

  // Tests if we think the server means for this mime_type to be executable.
  static bool IsExecutableMimeType(const std::string& mime_type);

  // Tests if a file is considered executable, based on its type.
  bool IsExecutableFile(const FilePath& path) const;

  // Tests if a file type is considered executable.
  static bool IsExecutableExtension(const FilePath::StringType& extension);

  // Resets the automatic open preference.
  void ResetAutoOpenFiles();

  // Returns true if there are automatic handlers registered for any file
  // types.
  bool HasAutoOpenFileTypesRegistered() const;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Deletes the specified path on the file thread.
  void DeleteDownload(const FilePath& path);

  // Called when the user has validated the donwload of a dangerous file.
  void DangerousDownloadValidated(DownloadItem* download);

  // Used to make sure we have a safe file extension and filename for a
  // download.  |file_name| can either be just the file name or it can be a
  // full path to a file.
  static void GenerateSafeFileName(const std::string& mime_type,
                                   FilePath* file_name);

  // Runs the network cancel.  Must be called on the IO thread.
  static void OnCancelDownloadRequest(ResourceDispatcherHost* rdh,
                                      int render_process_id,
                                      int request_id);

  // Create a file name based on the response from the server.
  static void GenerateFileName(const GURL& url,
                               const std::string& content_disposition,
                               const std::string& referrer_charset,
                               const std::string& mime_type,
                               FilePath* generated_name);

 private:
  class FakeDbHandleGenerator {
   public:
    explicit FakeDbHandleGenerator(int64 start_value) : value_(start_value) {}

    int64 GetNext() { return value_--; }
   private:
    int64 value_;
  };

  // This class is used to let an incognito DownloadManager observe changes to
  // a normal DownloadManager, to propagate ModelChanged() calls from the parent
  // DownloadManager to the observers of the incognito DownloadManager.
  class OtherDownloadManagerObserver : public Observer {
   public:
    explicit OtherDownloadManagerObserver(
        DownloadManager* observing_download_manager);
    virtual ~OtherDownloadManagerObserver();

    // Observer interface.
    virtual void ModelChanged();
    virtual void SetDownloads(std::vector<DownloadItem*>& downloads);
    virtual void ManagerGoingDown();

   private:
    // The incognito download manager.
    DownloadManager* observing_download_manager_;

    // The original profile's download manager.
    DownloadManager* observed_download_manager_;
  };

  friend class base::RefCountedThreadSafe<DownloadManager>;
  friend class OtherDownloadManagerObserver;

  ~DownloadManager();

  // Opens a download via the Windows shell.
  void OpenDownloadInShell(const DownloadItem* download,
                           gfx::NativeView parent_window);

  // Opens downloaded Chrome extension file (*.crx).
  void OpenChromeExtension(const FilePath& full_path,
                           const GURL& download_url,
                           const GURL& referrer_url,
                           const std::string& original_mime_type);

  // Shutdown the download manager.  This call is needed only after Init.
  void Shutdown();

  // Called on the download thread to check whether the suggested file path
  // exists.  We don't check if the file exists on the UI thread to avoid UI
  // stalls from interacting with the file system.
  void CheckIfSuggestedPathExists(DownloadCreateInfo* info);

  // Called on the UI thread once the DownloadManager has determined whether the
  // suggested file path exists.
  void OnPathExistenceAvailable(DownloadCreateInfo* info);

  // Called back after a target path for the file to be downloaded to has been
  // determined, either automatically based on the suggested file name, or by
  // the user in a Save As dialog box.
  void ContinueStartDownload(DownloadCreateInfo* info,
                             const FilePath& target_path);

  // Update the history service for a particular download.
  void UpdateHistoryForDownload(DownloadItem* download);
  void RemoveDownloadFromHistory(DownloadItem* download);
  void RemoveDownloadsFromHistoryBetween(const base::Time remove_begin,
                                         const base::Time remove_before);

  // Create an extension based on the file name and mime type.
  static void GenerateExtension(const FilePath& file_name,
                                const std::string& mime_type,
                                FilePath::StringType* generated_extension);

  // Create a file name based on the response from the server.
  static void GenerateFileNameFromInfo(DownloadCreateInfo* info,
                                       FilePath* generated_name);

  // Persist the automatic opening preference.
  void SaveAutoOpens();

  // Download cancel helper function.
  void DownloadCancelledInternal(int download_id,
                                 int render_process_id,
                                 int request_id);

  // Runs the pause on the IO thread.
  static void OnPauseDownloadRequest(ResourceDispatcherHost* rdh,
                                     int render_process_id,
                                     int request_id,
                                     bool pause);

  // Performs the last steps required when a download has been completed.
  // It is necessary to break down the flow when a download is finished as
  // dangerous downloads are downloaded to temporary files that need to be
  // renamed on the file thread first.
  // Invoked on the UI thread.
  void ContinueDownloadFinished(DownloadItem* download);

  // Renames a finished dangerous download from its temporary file name to its
  // real file name.
  // Invoked on the file thread.
  void ProceedWithFinishedDangerousDownload(int64 download_handle,
                                            const FilePath& path,
                                            const FilePath& original_name);

  // Invoked on the UI thread when a dangerous downloaded file has been renamed.
  void DangerousDownloadRenamed(int64 download_handle,
                                bool success,
                                const FilePath& new_path,
                                int new_path_uniquifier);

  // Checks whether a file represents a risk if downloaded.
  bool IsDangerous(const FilePath& file_name);

  // Updates the app icon about the overall download progress.
  void UpdateAppIcon();

  // Changes the paths and file name of the specified |download|, propagating
  // the change to the history system.
  void RenameDownload(DownloadItem* download, const FilePath& new_path);

  // Inform observers that the model has changed.
  void NotifyModelChanged();

  // 'downloads_' is map of all downloads in this profile. The key is the handle
  // returned by the history system, which is unique across sessions. This map
  // owns all the DownloadItems once they have been created in the history
  // system.
  //
  // 'in_progress_' is a map of all downloads that are in progress and that have
  // not yet received a valid history handle. The key is the ID assigned by the
  // ResourceDispatcherHost, which is unique for the current session. This map
  // does not own the DownloadItems.
  //
  // 'dangerous_finished_' is a map of dangerous download that have finished
  // but were not yet approved by the user.  Similarly to in_progress_, the key
  // is the ID assigned by the ResourceDispatcherHost and the map does not own
  // the DownloadItems.  It is used on shutdown to delete completed downloads
  // that have not been approved.
  //
  // When a download is created through a user action, the corresponding
  // DownloadItem* is placed in 'in_progress_' and remains there until it has
  // received a valid handle from the history system. Once it has a valid
  // handle, the DownloadItem* is placed in the 'downloads_' map. When the
  // download is complete, it is removed from 'in_progress_'. Downloads from
  // past sessions read from a persisted state from the history system are
  // placed directly into 'downloads_' since they have valid handles in the
  // history system.
  typedef base::hash_map<int64, DownloadItem*> DownloadMap;
  DownloadMap downloads_;
  DownloadMap in_progress_;
  DownloadMap dangerous_finished_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active profile.
  Profile* profile_;
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  // Used for history service request management.
  CancelableRequestConsumerTSimple<Observer*> cancelable_consumer_;

  // Non-owning pointer for handling file writing on the download_thread_.
  DownloadFileManager* file_manager_;

  // User preferences
  BooleanPrefMember prompt_for_download_;
  FilePathPrefMember download_path_;

  // The user's last choice for download directory. This is only used when the
  // user wants us to prompt for a save location for each download.
  FilePath last_download_path_;

  // Set of file extensions to open at download completion.
  struct AutoOpenCompareFunctor {
    inline bool operator()(const FilePath::StringType& a,
                           const FilePath::StringType& b) const {
      return FilePath::CompareLessIgnoreCase(a, b);
    }
  };
  typedef std::set<FilePath::StringType, AutoOpenCompareFunctor> AutoOpenSet;
  AutoOpenSet auto_open_;

  // Keep track of downloads that are completed before the user selects the
  // destination, so that observers are appropriately notified of completion
  // after this determination is made.
  // The map is of download_id->remaining size (bytes), both of which are
  // required when calling DownloadFinished.
  typedef std::map<int32, int64> PendingFinishedMap;
  PendingFinishedMap pending_finished_downloads_;

  // The "Save As" dialog box used to ask the user where a file should be
  // saved.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // In case we don't have a valid db_handle, we use |fake_db_handle_| instead.
  // This is useful for incognito mode or when the history database is offline.
  // Downloads are expected to have unique handles, so FakeDbHandleGenerator
  // automatically decrement the handle value on every use.
  FakeDbHandleGenerator fake_db_handle_;

  scoped_ptr<OtherDownloadManagerObserver> other_download_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
