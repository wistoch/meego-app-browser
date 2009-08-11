#ifndef LIBRARY_H__
#define LIBRARY_H__

#include <elf.h>
#include <map>
#include <set>
#include <string>
#include <string.h>
#include <sys/mman.h>

#include "maps.h"

#if defined(__x86_64__)
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Sym  Elf_Sym;
typedef Elf64_Addr Elf_Addr;
#elif defined(__i386__)
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Sym  Elf_Sym;
typedef Elf32_Addr Elf_Addr;
#else
#error Unsupported target platform
#endif

struct SyscallTable;
namespace playground {

class Library {
  friend class Maps;
 public:
  Library() :
      valid_(false),
      isVDSO_(false),
      asr_offset_(0),
      vsys_offset_(0),
      maps_(0) {
  }

  void addMemoryRange(void* start, void* stop, Elf_Addr offset, int prot,
                      int isVDSO) {
    memory_ranges_.insert(std::make_pair(offset, Range(start, stop, prot)));
    isVDSO_ = isVDSO;
  }

  char *get(Elf_Addr offset, char *buf, size_t len);
  std::string get(Elf_Addr offset);
  char *getOriginal(Elf_Addr offset, char *buf, size_t len);
  std::string getOriginal(Elf_Addr offset);

  template<class T>T* get(Elf_Addr offset, T* t) {
    if (!valid_) {
      memset(t, 0, sizeof(T));
      return NULL;
    }
    return reinterpret_cast<T *>(get(offset, reinterpret_cast<char *>(t),
                                     sizeof(T)));
  }

  template<class T>T* getOriginal(Elf_Addr offset, T* t) {
    if (!valid_) {
      memset(t, 0, sizeof(T));
      return false;
    }
    if (maps_) {
      return reinterpret_cast<T *>(maps_->forwardGetRequest(
          this, offset, reinterpret_cast<char *>(t), sizeof(T)));
    }
    return get(offset, t);
  }

  template<class T>bool set(void *addr, T* value) {
    if (!valid_) {
      return false;
    }
    *reinterpret_cast<T *>(addr) = *value;
    return true;
  }

  template<class T>bool set(Elf_Addr offset, T* value) {
    if (!valid_) {
      return false;
    }
    RangeMap::const_iterator iter = memory_ranges_.lower_bound(offset);
    if (iter == memory_ranges_.end()) {
      return false;
    }
    offset -= iter->first;
    if (offset >
        reinterpret_cast<char *>(iter->second.stop) -
        reinterpret_cast<char *>(iter->second.start) -
        sizeof(T)) {
      return false;
    }
    *reinterpret_cast<T *>(
        reinterpret_cast<char *>(iter->second.start) + offset) = *value;
    return true;
  }

  const Elf_Ehdr* getEhdr();
  const Elf_Shdr* getSection(const std::string& section);
  const int getSectionIndex(const std::string& section);
  void **getRelocation(const std::string& symbol);
  void *getSymbol(const std::string& symbol);
  void makeWritable(bool state) const;
  void patchSystemCalls();
  bool isVDSO() const { return isVDSO_; }

 protected:
  bool parseElf();
  bool parseSymbols();
  void recoverOriginalDataParent(Maps* maps);
  void recoverOriginalDataChild(const std::string& child);

 private:
  class GreaterThan : public std::binary_function<Elf_Addr, Elf_Addr, bool> {
   public:
    bool operator() (Elf_Addr s1, Elf_Addr s2) const {
      return s1 > s2;
    }
  };

  struct Range {
    Range(void* start, void* stop, int prot) :
        start(start), stop(stop), prot(prot) { }
    void* start;
    void* stop;
    int   prot;
  };

  typedef std::map<Elf_Addr, Range, GreaterThan> RangeMap;
  typedef std::map<std::string, std::pair<int, Elf_Shdr> > SectionTable;
  typedef std::map<std::string, Elf_Sym> SymbolTable;
  typedef std::map<std::string, Elf_Addr> PltTable;

  char* getBytes(char* dst, const char* src, ssize_t len);
  static bool isSafeInsn(unsigned short insn);
  static int isSimpleSystemCall(char *start, char *end);
  static char* getScratchSpace(const Maps* maps, char* near, int needed,
                               char** extraSpace, int* extraLength);
  void patchSystemCallsInFunction(const Maps* maps, char *start, char *end,
                                  char** extraSpace, int* extraLength);
  int  patchVSystemCalls();
  void patchVDSO(char** extraSpace, int* extraLength);

  RangeMap        memory_ranges_;
  bool            valid_;
  bool            isVDSO_;
  char*           asr_offset_;
  int             vsys_offset_;
  Maps*           maps_;
  Elf_Ehdr        ehdr_;
  SectionTable    section_table_;
  SymbolTable     symbols_;
  PltTable        plt_entries_;
  static char*    __kernel_vsyscall;
  static char*    __kernel_sigreturn;
  static char*    __kernel_rt_sigreturn;
};

} // namespace

#endif // LIBRARY_H__
