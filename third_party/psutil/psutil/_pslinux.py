#!/usr/bin/env python
#
# $Id: _pslinux.py 800 2010-11-12 21:51:25Z g.rodola $
#

__all__ = ["NUM_CPUS", "TOTAL_PHYMEM",
           "PlatformProcess",
           "avail_phymem", "used_phymem", "total_virtmem", "avail_virtmem",
           "used_virtmem", "get_system_cpu_times", "pid_exists", "get_pid_list",
           "phymem_buffers", "cached_phymem"
          ]


import os
import errno
import socket
import struct
import sys
import base64

try:
    from collections import namedtuple
except ImportError:
    from psutil.compat import namedtuple  # python < 2.6

from psutil import _psposix
from psutil.error import AccessDenied, NoSuchProcess


def _get_uptime():
    """Return system boot time (epoch in seconds)"""
    f = open('/proc/stat', 'r')
    for line in f:
        if line.startswith('btime'):
            f.close()
            return float(line.strip().split()[1])

def _get_num_cpus():
    """Return the number of CPUs on the system"""
    num = 0
    f = open('/proc/cpuinfo', 'r')
    for line in f:
        if line.startswith('processor'):
            num += 1
    f.close()
    return num

def _get_total_phymem():
    """Return the total amount of physical memory, in bytes"""
    f = open('/proc/meminfo', 'r')
    for line in f:
        if line.startswith('MemTotal:'):
            f.close()
            return int(line.split()[1]) * 1024


# Number of clock ticks per second
_CLOCK_TICKS = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
_UPTIME = _get_uptime()
NUM_CPUS = _get_num_cpus()
TOTAL_PHYMEM = _get_total_phymem()

del _get_uptime, _get_num_cpus, _get_total_phymem

# http://students.mimuw.edu.pl/lxr/source/include/net/tcp_states.h
_TCP_STATES_TABLE = {"01" : "ESTABLISHED",
                     "02" : "SYN_SENT",
                     "03" : "SYN_RECV",
                     "04" : "FIN_WAIT1",
                     "05" : "FIN_WAIT2",
                     "06" : "TIME_WAIT",
                     "07" : "CLOSE",
                     "08" : "CLOSE_WAIT",
                     "09" : "LAST_ACK",
                     "0A" : "LISTEN",
                     "0B" : "CLOSING"
                     }

def avail_phymem():
    """Return the amount of physical memory available, in bytes."""
    f = open('/proc/meminfo', 'r')
    free = None
    _flag = False
    for line in f:
        if line.startswith('MemFree:'):
            free = int(line.split()[1]) * 1024
            break
    f.close()
    return free

def used_phymem():
    """"Return the amount of physical memory used, in bytes."""
    return (TOTAL_PHYMEM - avail_phymem())

def total_virtmem():
    """"Return the total amount of virtual memory, in bytes."""
    f = open('/proc/meminfo', 'r')
    for line in f:
        if line.startswith('SwapTotal:'):
            f.close()
            return int(line.split()[1]) * 1024

def avail_virtmem():
    """Return the amount of virtual memory currently in use on the
    system, in bytes.
    """
    f = open('/proc/meminfo', 'r')
    for line in f:
        if line.startswith('SwapFree:'):
            f.close()
            return int(line.split()[1]) * 1024

def used_virtmem():
    """Return the amount of used memory currently in use on the system,
    in bytes.
    """
    return total_virtmem() - avail_virtmem()

def cached_phymem():
    """Return the amount of cached memory on the system, in bytes.
    This reflects the "cached" column of free command line utility.
    """
    f = open('/proc/meminfo', 'r')
    for line in f:
        if line.startswith('Cached:'):
            f.close()
            return int(line.split()[1]) * 1024

def phymem_buffers():
    """Return the amount of physical memory buffers used by the
    kernel in bytes.
    This reflects the "buffers" column of free command line utility.
    """
    f = open('/proc/meminfo', 'r')
    for line in f:
        if line.startswith('Buffers:'):
            f.close()
            return int(line.split()[1]) * 1024

def get_system_cpu_times():
    """Return a dict representing the following CPU times:
    user, nice, system, idle, iowait, irq, softirq.
    """
    f = open('/proc/stat', 'r')
    values = f.readline().split()
    f.close()

    values = values[1:8]
    values = tuple([float(x) / _CLOCK_TICKS for x in values])

    return dict(user=values[0], nice=values[1], system=values[2], idle=values[3],
                iowait=values[4], irq=values[5], softirq=values[6])

def get_pid_list():
    """Returns a list of PIDs currently running on the system."""
    pids = [int(x) for x in os.listdir('/proc') if x.isdigit()]
    # special case for 0 (kernel process) PID
    pids.insert(0, 0)
    return pids

def pid_exists(pid):
    """Check For the existence of a unix pid."""
    return _psposix.pid_exists(pid)


# --- decorators

def wrap_exceptions(callable):
    """Call callable into a try/except clause and translate ENOENT,
    EACCES and EPERM in NoSuchProcess or AccessDenied exceptions.
    """
    def wrapper(self, *args, **kwargs):
        try:
            return callable(self, *args, **kwargs)
        except (OSError, IOError), err:
            if err.errno == errno.ENOENT:  # no such file or directory
                raise NoSuchProcess(self.pid, self._process_name)
            if err.errno in (errno.EPERM, errno.EACCES):
                raise AccessDenied(self.pid, self._process_name)
            raise
    return wrapper


class LinuxProcess(object):
    """Linux process implementation."""

    _meminfo_ntuple = namedtuple('meminfo', 'rss vms')
    _cputimes_ntuple = namedtuple('cputimes', 'user system')
    _openfile_ntuple = namedtuple('openfile', 'path fd')
    _connection_ntuple = namedtuple('connection', 'fd family type local_address '
                                                  'remote_address status')
    __slots__ = ["pid", "_process_name"]

    def __init__(self, pid):
        self.pid = pid
        self._process_name = None

    @wrap_exceptions
    def get_process_name(self):
        if self.pid == 0:
            return 'sched'  # special case for kernel process
        f = open("/proc/%s/stat" % self.pid)
        try:
            name = f.read().split(' ')[1].replace('(', '').replace(')', '')
        finally:
            f.close()
        # XXX - gets changed later and probably needs refactoring
        return name

    def get_process_exe(self):
        if self.pid in (0, 2):
            return ""   # special case for kernel processes
        try:
            exe = os.readlink("/proc/%s/exe" % self.pid)
        except (OSError, IOError), err:
            if err.errno == errno.ENOENT:
                # no such file error; might be raised also if the
                # path actually exists for system processes with
                # low pids (about 0-20)
                if os.path.lexists("/proc/%s/exe" % self.pid):
                    return ""
                else:
                    # ok, it is a process which has gone away
                    raise NoSuchProcess(self.pid, self._process_name)
            if err.errno in (errno.EPERM, errno.EACCES):
                raise AccessDenied(self.pid, self._process_name)
            raise

        # It seems symlinks can point to a deleted/invalid location
        # (this usually  happens with "pulseaudio" process).
        # However, if we had permissions to execute readlink() it's
        # likely that we'll be able to figure out exe from argv[0]
        # later on.
        if exe.endswith(" (deleted)") and not os.path.isfile(exe):
            return ""
        return exe

    @wrap_exceptions
    def get_process_cmdline(self):
        if self.pid == 0:
            return []   # special case for kernel process
        f = open("/proc/%s/cmdline" % self.pid)
        try:
            # return the args as a list
            return [x for x in f.read().split('\x00') if x]
        finally:
            f.close()

    @wrap_exceptions
    def get_cpu_times(self):
        # special case for 0 (kernel process) PID
        if self.pid == 0:
            return self._cputimes_ntuple(0.0, 0.0)
        f = open("/proc/%s/stat" % self.pid)
        st = f.read().strip()
        f.close()
        # ignore the first two values ("pid (exe)")
        st = st[st.find(')') + 2:]
        values = st.split(' ')
        utime = float(values[11]) / _CLOCK_TICKS
        stime = float(values[12]) / _CLOCK_TICKS
        return self._cputimes_ntuple(utime, stime)

    @wrap_exceptions
    def get_process_create_time(self):
        # special case for 0 (kernel processes) PID; return system uptime
        if self.pid == 0:
            return _UPTIME
        f = open("/proc/%s/stat" % self.pid)
        st = f.read().strip()
        f.close()
        # ignore the first two values ("pid (exe)")
        st = st[st.find(')') + 2:]
        values = st.split(' ')
        # According to documentation, starttime is in field 21 and the
        # unit is jiffies (clock ticks).
        # We first divide it for clock ticks and then add uptime returning
        # seconds since the epoch, in UTC.
        starttime = (float(values[19]) / _CLOCK_TICKS) + _UPTIME
        return starttime

    @wrap_exceptions
    def get_memory_info(self):
        # special case for 0 (kernel processes) PID
        if self.pid == 0:
            return self._meminfo_ntuple(0, 0)
        f = open("/proc/%s/status" % self.pid)
        virtual_size = 0
        resident_size = 0
        _flag = False
        for line in f:
            if (not _flag) and line.startswith("VmSize:"):
                virtual_size = int(line.split()[1]) * 1024
                _flag = True
            elif line.startswith("VmRSS"):
                resident_size = int(line.split()[1]) * 1024
                break
        f.close()
        return self._meminfo_ntuple(resident_size, virtual_size)

    @wrap_exceptions
    def get_process_cwd(self):
        if self.pid == 0:
            return ''
        return os.readlink("/proc/%s/cwd" % self.pid)

    @wrap_exceptions
    def get_process_num_threads(self):
        if self.pid == 0:
            return 0
        f = open("/proc/%s/status" % self.pid)
        for line in f:
            if line.startswith("Threads:"):
                f.close()
                return int(line.split()[1])

    @wrap_exceptions
    def get_open_files(self):
        retlist = []
        files = os.listdir("/proc/%s/fd" % self.pid)
        for fd in files:
            file = "/proc/%s/fd/%s" % (self.pid, fd)
            if os.path.islink(file):
                file = os.readlink(file)
                if file.startswith("socket:["):
                    continue
                if file.startswith("pipe:["):
                    continue
                if file == "[]":
                    continue
                if os.path.isfile(file) and not file in retlist:
                    ntuple = self._openfile_ntuple(file, int(fd))
                    retlist.append(ntuple)
        return retlist

#    --- lsof implementation
#
#    def get_open_files(self):
#        lsof = _psposix.LsofParser(self.pid, self._process_name)
#        return lsof.get_process_open_files()

    @wrap_exceptions
    def get_connections(self):
        if self.pid == 0:
            return []
        inodes = {}
        # os.listdir() is gonna raise a lot of access denied
        # exceptions in case of unprivileged user; that's fine:
        # lsof does the same so it's unlikely that we can to better.
        for fd in os.listdir("/proc/%s/fd" % self.pid):
            try:
                inode = os.readlink("/proc/%s/fd/%s" % (self.pid, fd))
            except OSError:
                continue
            if inode.startswith('socket:['):
                # the process is using a socket
                inode = inode[8:][:-1]
                inodes[inode] = fd

        if not inodes:
            # no connections for this process
            return []

        def process(file, family, _type):
            retlist = []
            f = open(file)
            f.readline()  # skip the first line
            for line in f:
                _, laddr, raddr, status, _, _, _, _, _, inode = line.split()[:10]
                if inode in inodes:
                    laddr = self._decode_address(laddr, family)
                    raddr = self._decode_address(raddr, family)
                    if _type == socket.SOCK_STREAM:
                        status = _TCP_STATES_TABLE[status]
                    else:
                        status = ""
                    fd = int(inodes[inode])
                    conn = self._connection_ntuple(fd, family, _type, laddr,
                                                   raddr, status)
                    retlist.append(conn)
            f.close()
            return retlist

        tcp4 = process("/proc/net/tcp", socket.AF_INET, socket.SOCK_STREAM)
        tcp6 = process("/proc/net/tcp6", socket.AF_INET6, socket.SOCK_STREAM)
        udp4 = process("/proc/net/udp", socket.AF_INET, socket.SOCK_DGRAM)
        udp6 = process("/proc/net/udp6", socket.AF_INET6, socket.SOCK_DGRAM)
        return tcp4 + tcp6 + udp4 + udp6

#    --- lsof implementation
#
#    def get_connections(self):
#        lsof = _psposix.LsofParser(self.pid, self._process_name)
#        return lsof.get_process_connections()

    @wrap_exceptions
    def get_process_ppid(self):
        if self.pid == 0:
            return 0
        f = open("/proc/%s/status" % self.pid)
        for line in f:
            if line.startswith("PPid:"):
                # PPid: nnnn
                f.close()
                return int(line.split()[1])

    @wrap_exceptions
    def get_process_uid(self):
        if self.pid == 0:
            return 0
        f = open("/proc/%s/status" % self.pid)
        for line in f:
            if line.startswith('Uid:'):
                # Uid line provides 4 values which stand for real,
                # effective, saved set, and file system UIDs.
                # We want to provide real UID only.
                f.close()
                return int(line.split()[1])

    @wrap_exceptions
    def get_process_gid(self):
        if self.pid == 0:
            return 0
        f = open("/proc/%s/status" % self.pid)
        for line in f:
            if line.startswith('Gid:'):
                # Uid line provides 4 values which stand for real,
                # effective, saved set, and file system GIDs.
                # We want to provide real GID only.
                f.close()
                return int(line.split()[1])

    @staticmethod
    def _decode_address(addr, family):
        """Accept an "ip:port" address as displayed in /proc/net/*
        and convert it into a human readable form, like:

        "0500000A:0016" -> ("10.0.0.5", 22)
        "0000000000000000FFFF00000100007F:9E49" -> ("::ffff:127.0.0.1", 40521)

        The IPv4 address portion is a little-endian four-byte hexadecimal
        number; that is, the least significant byte is listed first,
        so we need to reverse the order of the bytes to convert it
        to an IP address.
        The port is represented as a two-byte hexadecimal number.

        Reference:
        http://linuxdevcenter.com/pub/a/linux/2000/11/16/LinuxAdmin.html
        """
        ip, port = addr.split(':')
        port = int(port, 16)
        if sys.version_info >= (3,):
            ip = ip.encode('ascii')
        # this usually refers to a local socket in listen mode with
        # no end-points connected
        if not port:
            return ()
        if family == socket.AF_INET:
            ip = socket.inet_ntop(family, base64.b16decode(ip)[::-1])
        else:  # IPv6
            # old version - let's keep it, just in case...
            #ip = ip.decode('hex')
            #return socket.inet_ntop(socket.AF_INET6,
            #          ''.join(ip[i:i+4][::-1] for i in xrange(0, 16, 4)))
            ip = base64.b16decode(ip)
            ip = socket.inet_ntop(socket.AF_INET6,
                                struct.pack('>4I', *struct.unpack('<4I', ip)))
        return (ip, port)

PlatformProcess = LinuxProcess

