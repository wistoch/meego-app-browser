/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// File declaring NPObjectProxy class.  This class wraps the NPAPI scripting
// interface with a COM IDispatchEx interface to allow interop between ActiveX
// and NPObject instances.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_OBJECT_PROXY_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_OBJECT_PROXY_H_

#include <atlctl.h>
#include <dispex.h>

// File included without directory because it is auto-generated by the
// type-lib.
#include "npapi_host_control.h"

#include "third_party/npapi/files/include/npupp.h"

struct NPObject;
class NPBrowserProxy;

// COM class implementing a basic IDispatchEx interface that wraps the NPAPI
// NPObject scripting functionality.
class ATL_NO_VTABLE NPObjectProxy :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<NPObjectProxy, &CLSID_NPObjectProxy>,
    public IDispatchImpl<INPObjectProxy, &IID_INPObjectProxy,
                         &LIBID_npapi_host_controlLib>,
    public IObjectSafetyImpl<NPObjectProxy,
                             INTERFACESAFE_FOR_UNTRUSTED_CALLER> {
 public:
  NPObjectProxy();
  virtual ~NPObjectProxy();

DECLARE_REGISTRY_RESOURCEID(IDR_NPOBJECTPROXY)

BEGIN_COM_MAP(NPObjectProxy)
  COM_INTERFACE_ENTRY(INPObjectProxy)
  COM_INTERFACE_ENTRY(IDispatch)
  COM_INTERFACE_ENTRY(IDispatchEx)
END_COM_MAP()

  STDMETHOD(SetBrowserProxy)(void* browser_proxy) {
    browser_proxy_ = static_cast<NPBrowserProxy*>(browser_proxy);
    return S_OK;
  }

  // Routine implementing INPObjectProxy interface method, returning a raw
  // pointer to a NPObject instance.  Note that the reference count of the
  // returned NPObject has been incremented.  The returned object should
  // be released by the hosting browser proxy to prevent memory leaks.
  STDMETHOD(GetNPObjectInstance)(void **np_instance);
  STDMETHOD(SetHostedObject)(void* hosted_object);
  STDMETHOD(ReleaseHosted)();

  // Routines implementing the IDispatchEx COM interface.
  STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
  STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
  STDMETHOD(GetIDsOfNames)(REFIID riid,
                           LPOLESTR* rgszNames,
                           UINT cNames,
                           LCID lcid,
                           DISPID* rgdispid);
  STDMETHOD(Invoke)(DISPID dispidMember,
                    REFIID riid,
                    LCID lcid,
                    WORD wFlags,
                    DISPPARAMS* pdispparams,
                    VARIANT* pvarResult,
                    EXCEPINFO* pexcepinfo,
                    UINT* puArgErr);

  STDMETHOD(DeleteMemberByDispID)(DISPID id);
  STDMETHOD(DeleteMemberByName)(BSTR bstrName, DWORD grfdex);
  STDMETHOD(GetDispID)(BSTR bstrName, DWORD grfdex, DISPID* pid);
  STDMETHOD(GetMemberName)(DISPID id, BSTR* pbstrName);
  STDMETHOD(GetMemberProperties)(DISPID id, DWORD grfdexFetch, DWORD* pgrfdex);
  STDMETHOD(GetNameSpaceParent)(IUnknown** ppunk);
  STDMETHOD(GetNextDispID)(DWORD grfdex, DISPID id, DISPID* pid);
  STDMETHOD(InvokeEx)(DISPID id, LCID lcid, WORD wFlags, DISPPARAMS* pdp,
                      VARIANT* pVarRes, EXCEPINFO* pei,
                      IServiceProvider* pspCaller);

  DECLARE_PROTECT_FINAL_CONSTRUCT();
 private:
  bool HasPropertyOrMethod(NPIdentifier np_identifier);

  // Pointer to NPObject for which this instance is a proxy IDispatchEx.
  NPObject *hosted_;

  // Back-pointer to the NPAPI browser proxy.
  NPBrowserProxy* browser_proxy_;

  DISALLOW_COPY_AND_ASSIGN(NPObjectProxy);
};

// Register this COM class with the COM module.
OBJECT_ENTRY_AUTO(__uuidof(NPObjectProxy), NPObjectProxy);

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_OBJECT_PROXY_H_
