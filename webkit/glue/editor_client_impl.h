// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__
#define WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__

#include "base/compiler_specific.h"
#include "base/task.h"

#include "build/build_config.h"

#include <deque>

MSVC_PUSH_WARNING_LEVEL(0);
#include "EditorClient.h"
MSVC_POP_WARNING();

namespace WebCore {
class Frame;
class HTMLInputElement;
class Node;
class PlatformKeyboardEvent;
}

class WebView;
class WebViewImpl;

class EditorClientImpl : public WebCore::EditorClient {
 public:
  EditorClientImpl(WebView* web_view);
  virtual ~EditorClientImpl();
  virtual void pageDestroyed();

  virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
  virtual bool smartInsertDeleteEnabled();
  virtual bool isContinuousSpellCheckingEnabled();
  virtual void toggleContinuousSpellChecking();
  virtual bool isGrammarCheckingEnabled();
  virtual void toggleGrammarChecking();
  virtual int spellCheckerDocumentTag();

  virtual bool isEditable();

  virtual bool shouldBeginEditing(WebCore::Range* range);
  virtual bool shouldEndEditing(WebCore::Range* range);
  virtual bool shouldInsertNode(WebCore::Node* node, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldInsertText(const WebCore::String& text, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldDeleteRange(WebCore::Range* range);
  virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange, 
                                         WebCore::Range* toRange, 
                                         WebCore::EAffinity affinity, 
                                         bool stillSelecting);
  virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                WebCore::Range* range);
  virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);

  virtual void didBeginEditing();
  virtual void respondToChangedContents();
  virtual void respondToChangedSelection();
  virtual void didEndEditing();
  virtual void didWriteSelectionToPasteboard();
  virtual void didSetSelectionTypesForPasteboard();

  virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
  virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
  virtual void clearUndoRedoOperations();

  virtual bool canUndo() const;
  virtual bool canRedo() const;

  virtual void undo();
  virtual void redo();

  virtual const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
  virtual bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*);

  virtual void textFieldDidBeginEditing(WebCore::Element*);
  virtual void textFieldDidEndEditing(WebCore::Element*);
  virtual void textDidChangeInTextField(WebCore::Element*);
  virtual bool doTextFieldCommandFromEvent(WebCore::Element*,WebCore::KeyboardEvent*);
  virtual void textWillBeDeletedInTextField(WebCore::Element*);
  virtual void textDidChangeInTextArea(WebCore::Element*);

  virtual void ignoreWordInSpellDocument(const WebCore::String&);
  virtual void learnWord(const WebCore::String&);
  virtual void checkSpellingOfString(const UChar*, int length,
                                     int* misspellingLocation,
                                     int* misspellingLength);
  virtual void checkGrammarOfString(const UChar*, int length,
                                    WTF::Vector<WebCore::GrammarDetail>&,
                                    int* badGrammarLocation,
                                    int* badGrammarLength);
  virtual void updateSpellingUIWithGrammarString(const WebCore::String&,
                                                 const WebCore::GrammarDetail& detail);
  virtual void updateSpellingUIWithMisspelledWord(const WebCore::String&);
  virtual void showSpellingUI(bool show);
  virtual bool spellingUIIsShowing();
  virtual void getGuessesForWord(const WebCore::String&,
                                 WTF::Vector<WebCore::String>& guesses);
  virtual void setInputMethodState(bool enabled);

  void SetUseEditorDelegate(bool value) { use_editor_delegate_ = value; }

  // It would be better to add these methods to the objects they describe, but
  // those are in WebCore and therefore inaccessible.
  virtual std::wstring DescribeOrError(int number, 
                                       WebCore::ExceptionCode ec);
  virtual std::wstring DescribeOrError(WebCore::Node* node, 
                                       WebCore::ExceptionCode ec);
  virtual std::wstring Describe(WebCore::Range* range);
  virtual std::wstring Describe(WebCore::Node* node);
  virtual std::wstring Describe(WebCore::EditorInsertAction action);
  virtual std::wstring Describe(WebCore::EAffinity affinity);
  virtual std::wstring Describe(WebCore::CSSStyleDeclaration* style);

 private:
  void ModifySelection(WebCore::Frame* frame,
                       WebCore::KeyboardEvent* event);

  void DoAutofill(WebCore::HTMLInputElement* input_element, bool backspace);

 protected:
  WebViewImpl* web_view_;
  bool use_editor_delegate_;
  bool in_redo_;

  typedef std::deque<WTF::RefPtr<WebCore::EditCommand> > EditCommandStack;
  EditCommandStack undo_stack_;
  EditCommandStack redo_stack_;

 private:
  // Whether the last entered key was a backspace.
  bool backspace_pressed_;

  // The method factory used to post autofill related tasks.
  ScopedRunnableMethodFactory<EditorClientImpl> autofill_factory_;
};

#endif  // WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__

