// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Mac interface forwards most of these commands to the application layer,
// and I'm not really sure what to do about most of them.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Document.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "KeyboardCodes.h"
#include "HTMLInputElement.h"
#include "Frame.h"
#include "KeyboardEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
MSVC_POP_WARNING();

#undef LOG
#include "base/string_util.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webview_impl.h"

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory.  This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
static const size_t kMaximumUndoStackDepth = 1000;

namespace {

// Record an editor command from the keyDownEntries[] below.  We ignore the
// Move* and Insert* commands because they're not that interesting.
void MaybeRecordCommand(WebViewDelegate* d, const char* command_name) {
  if (!d)
    return;

  const char* move_prefix = "Move";
  const char* insert_prefix = "Insert";
  const char* delete_prefix = "Delete";
  // Ignore all the Move*, Insert*, and Delete* commands.
  if (0 == strncmp(command_name, move_prefix, sizeof(move_prefix)) ||
      0 == strncmp(command_name, insert_prefix, sizeof(insert_prefix)) ||
      0 == strncmp(command_name, delete_prefix, sizeof(delete_prefix))) {
    return;
  }
  d->UserMetricsRecordComputedAction(UTF8ToWide(command_name));
}

}

EditorClientImpl::EditorClientImpl(WebView* web_view)
    : web_view_(static_cast<WebViewImpl*>(web_view)),
      use_editor_delegate_(false),
      in_redo_(false),
      preserve_(false),
      pending_inline_autocompleted_element_(NULL) {
}

EditorClientImpl::~EditorClientImpl() {
}

void EditorClientImpl::pageDestroyed() {
  // Called by the Page (which owns the editor client) when the page is going
  // away. This should cause us to delete ourselves, which is stupid. The page
  // should just delete us when it's going away. Oh well.
  delete this;
}

bool EditorClientImpl::shouldShowDeleteInterface(WebCore::HTMLElement* elem) {
  // Normally, we don't care to show WebCore's deletion UI, so we only enable
  // it if in testing mode and the test specifically requests it by using this
  // magic class name.
  return webkit_glue::IsLayoutTestMode() &&
         elem->className() == "needsDeletionUI";
}

bool EditorClientImpl::smartInsertDeleteEnabled() {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      return d->SmartInsertDeleteEnabled();
  }
  return true;
}

bool EditorClientImpl::isContinuousSpellCheckingEnabled() {
  // Spell check everything if possible.
  // FIXME(brettw) This should be modified to do reasonable defaults depending
  // on input type, and probably also allow the user to turn spellchecking on
  // for individual fields.
  return true;
}

void EditorClientImpl::toggleContinuousSpellChecking() {
  NOTIMPLEMENTED();
}

bool EditorClientImpl::isGrammarCheckingEnabled() {
  return false;
}

void EditorClientImpl::toggleGrammarChecking() {
  NOTIMPLEMENTED();
}

int EditorClientImpl::spellCheckerDocumentTag() {
  NOTIMPLEMENTED();
  return 0;
}

bool EditorClientImpl::isEditable() {
  return false;
}

bool EditorClientImpl::shouldBeginEditing(WebCore::Range* range) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      return d->ShouldBeginEditing(web_view_, Describe(range));
  }
  return true;
}

bool EditorClientImpl::shouldEndEditing(WebCore::Range* range) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      return d->ShouldEndEditing(web_view_, Describe(range));
  }
  return true;
}

bool EditorClientImpl::shouldInsertNode(WebCore::Node* node,
                                        WebCore::Range* range,
                                        WebCore::EditorInsertAction action) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d) {
      return d->ShouldInsertNode(web_view_,
                                 Describe(node),
                                 Describe(range),
                                 Describe(action));
    }
  }
  return true;
}

bool EditorClientImpl::shouldInsertText(const WebCore::String& text,
                                        WebCore::Range* range,
                                        WebCore::EditorInsertAction action) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d) {
      std::wstring wstr = webkit_glue::StringToStdWString(text);
      return d->ShouldInsertText(web_view_, 
                                 wstr, 
                                 Describe(range), 
                                 Describe(action));
    }
  }
  return true;
}


bool EditorClientImpl::shouldDeleteRange(WebCore::Range* range) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      return d->ShouldDeleteRange(web_view_, Describe(range));
  }
  return true;
}

void EditorClientImpl::PreserveSelection() {
  preserve_ = true;
}

bool EditorClientImpl::shouldChangeSelectedRange(WebCore::Range* fromRange, 
                                                 WebCore::Range* toRange, 
                                                 WebCore::EAffinity affinity, 
                                                 bool stillSelecting) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d) {
      return d->ShouldChangeSelectedRange(web_view_, 
                                          Describe(fromRange), 
                                          Describe(toRange), 
                                          Describe(affinity), 
                                          stillSelecting);
    }
  }
  // Have we been told to preserve the selection? 
  // (See comments for PreserveSelection in header).
  if (preserve_) {
    preserve_ = false;
    return false;
  }
  return true;
}
                                                
bool EditorClientImpl::shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                        WebCore::Range* range) {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      return d->ShouldApplyStyle(web_view_, Describe(style), Describe(range));
  }
  return true;
}

bool EditorClientImpl::shouldMoveRangeAfterDelete(
    WebCore::Range* /*range*/,
    WebCore::Range* /*rangeToBeReplaced*/) {
  return true;
}

void EditorClientImpl::didBeginEditing() {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      d->DidBeginEditing();
  }
}

void EditorClientImpl::respondToChangedSelection() {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      d->DidChangeSelection();
  }
}

void EditorClientImpl::respondToChangedContents() {
  // Ugly Hack. (See also webkit bug #16976).
  // Something is wrong with webcore's focusController in that when selection
  // is set to a region within a text element when handling an input event, if
  // you don't re-focus the node then it only _APPEARS_ to have successfully
  // changed the selection (the UI "looks" right) but in reality there is no
  // selection of text. And to make matters worse, you can't just re-focus it,
  // you have to re-focus it in code executed after the entire event listener 
  // loop has finished; and hence here we are. Oh, and to make matters worse, 
  // this sequence of events _doesn't_ happen when you debug through the code 
  // -- in that case it works perfectly fine -- because swapping to the debugger 
  // causes the refocusing we artificially reproduce here.
  // TODO (timsteele): Clean this up once root webkit problem is identified and
  // the bug is patched. 
  if (pending_inline_autocompleted_element_) {
    pending_inline_autocompleted_element_->blur();
    pending_inline_autocompleted_element_->focus();
    pending_inline_autocompleted_element_ = NULL; 
  }

  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      d->DidChangeContents();
  }
}

void EditorClientImpl::didEndEditing() {
  if (use_editor_delegate_) {
    WebViewDelegate* d = web_view_->delegate();
    if (d)
      d->DidEndEditing();
  }
}

void EditorClientImpl::didWriteSelectionToPasteboard() {
}

void EditorClientImpl::didSetSelectionTypesForPasteboard() {
}

void EditorClientImpl::registerCommandForUndo(
    PassRefPtr<WebCore::EditCommand> command) {
  if (undo_stack_.size() == kMaximumUndoStackDepth)
    undo_stack_.pop_front();  // drop oldest item off the far end
  if (!in_redo_)
    redo_stack_.clear();
  undo_stack_.push_back(command);
}

void EditorClientImpl::registerCommandForRedo(
    PassRefPtr<WebCore::EditCommand> command) {
  redo_stack_.push_back(command);
}

void EditorClientImpl::clearUndoRedoOperations() {
  undo_stack_.clear();
  redo_stack_.clear();
}

bool EditorClientImpl::canUndo() const {
  return !undo_stack_.empty();
}

bool EditorClientImpl::canRedo() const {
  return !redo_stack_.empty();
}

void EditorClientImpl::undo() {
  if (canUndo()) {
    RefPtr<WebCore::EditCommand> command(undo_stack_.back());
    undo_stack_.pop_back();
    command->unapply();
    // unapply will call us back to push this command onto the redo stack.
  }
}

void EditorClientImpl::redo() {
  if (canRedo()) {
    RefPtr<WebCore::EditCommand> command(redo_stack_.back());
    redo_stack_.pop_back();

    ASSERT(!in_redo_);
    in_redo_ = true;
    command->reapply();
    // reapply will call us back to push this command onto the undo stack.
    in_redo_ = false;
  }
}

// The below code was adapted from the WebKit file webview.cpp
// provided by Apple, Inc, and is subject to the following copyright
// notice and disclaimer.

/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;
static const unsigned MetaKey = 1 << 3;
#if defined(OS_MACOSX)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
static const unsigned OptionKey  = AltKey;
static const unsigned CommandKey = MetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyDownEntry {
  unsigned virtualKey;
  unsigned modifiers;
  const char* name;
};

struct KeyPressEntry {
  unsigned charCode;
  unsigned modifiers;
  const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
  { WebCore::VKEY_LEFT,   0,                  "MoveLeft"                      },
  { WebCore::VKEY_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"    },
#if defined(OS_MACOSX)
  { WebCore::VKEY_LEFT,   OptionKey,          "MoveWordLeft"                  },
  { WebCore::VKEY_LEFT,   OptionKey | ShiftKey, 
        "MoveWordLeftAndModifySelection"                                      },
#else
  { WebCore::VKEY_LEFT,   CtrlKey,            "MoveWordLeft"                  },
  { WebCore::VKEY_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"},
#endif
  { WebCore::VKEY_RIGHT,  0,                  "MoveRight"                     },
  { WebCore::VKEY_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"   },
#if defined(OS_MACOSX)
  { WebCore::VKEY_RIGHT,  OptionKey,          "MoveWordRight"                 },
  { WebCore::VKEY_RIGHT,  OptionKey | ShiftKey,
        "MoveWordRightAndModifySelection"                                     },
#else
  { WebCore::VKEY_RIGHT,  CtrlKey,            "MoveWordRight"                 },
  { WebCore::VKEY_RIGHT,  CtrlKey | ShiftKey,
        "MoveWordRightAndModifySelection"                                     },
#endif
  { WebCore::VKEY_UP,     0,                  "MoveUp"                        },
  { WebCore::VKEY_UP,     ShiftKey,           "MoveUpAndModifySelection"      },
  { WebCore::VKEY_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"  },
  { WebCore::VKEY_DOWN,   0,                  "MoveDown"                      },
  { WebCore::VKEY_DOWN,   ShiftKey,           "MoveDownAndModifySelection"    },
  { WebCore::VKEY_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"},
  { WebCore::VKEY_PRIOR,  0,                  "MovePageUp"                    },
  { WebCore::VKEY_NEXT,   0,                  "MovePageDown"                  },
  { WebCore::VKEY_HOME,   0,                  "MoveToBeginningOfLine"         },
  { WebCore::VKEY_HOME,   ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#if defined(OS_MACOSX)
  { WebCore::VKEY_LEFT,   CommandKey,         "MoveToBeginningOfLine"         },
  { WebCore::VKEY_LEFT,   CommandKey | ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#endif
#if defined(OS_MACOSX)
  { WebCore::VKEY_UP,     CommandKey,         "MoveToBeginningOfDocument"     },
  { WebCore::VKEY_UP,     CommandKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },  
#else
  { WebCore::VKEY_HOME,   CtrlKey,            "MoveToBeginningOfDocument"     },
  { WebCore::VKEY_HOME,   CtrlKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#endif
  { WebCore::VKEY_END,    0,                  "MoveToEndOfLine"               },
  { WebCore::VKEY_END,    ShiftKey,
        "MoveToEndOfLineAndModifySelection"                                   },
#if defined(OS_MACOSX)
  { WebCore::VKEY_DOWN,   CommandKey,         "MoveToEndOfDocument"           },
  { WebCore::VKEY_DOWN,   CommandKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#else
  { WebCore::VKEY_END,    CtrlKey,            "MoveToEndOfDocument"           },
  { WebCore::VKEY_END,    CtrlKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#endif
#if defined(OS_MACOSX)
  { WebCore::VKEY_RIGHT,  CommandKey,            "MoveToEndOfLine"            },
  { WebCore::VKEY_RIGHT,  CommandKey | ShiftKey,
        "MoveToEndOfLineAndModifySelection"                                   },
#endif
  { WebCore::VKEY_BACK,   0,                  "DeleteBackward"                },
  { WebCore::VKEY_BACK,   ShiftKey,           "DeleteBackward"                },
  { WebCore::VKEY_DELETE, 0,                  "DeleteForward"                 },
#if defined(OS_MACOSX)
  { WebCore::VKEY_BACK,   OptionKey,          "DeleteWordBackward"            },
  { WebCore::VKEY_DELETE, OptionKey,          "DeleteWordForward"             },
#else
  { WebCore::VKEY_BACK,   CtrlKey,            "DeleteWordBackward"            },
  { WebCore::VKEY_DELETE, CtrlKey,            "DeleteWordForward"             },
#endif
  { 'B',                  CtrlKey,            "ToggleBold"                    },
  { 'I',                  CtrlKey,            "ToggleItalic"                  },
  { 'U',                  CtrlKey,            "ToggleUnderline"               },
  { WebCore::VKEY_ESCAPE, 0,                  "Cancel"                        },
  { WebCore::VKEY_OEM_PERIOD, CtrlKey,        "Cancel"                        },
  { WebCore::VKEY_TAB,    0,                  "InsertTab"                     },
  { WebCore::VKEY_TAB,    ShiftKey,           "InsertBacktab"                 },
  { WebCore::VKEY_RETURN, 0,                  "InsertNewline"                 },
  { WebCore::VKEY_RETURN, CtrlKey,            "InsertNewline"                 },
  { WebCore::VKEY_RETURN, AltKey,             "InsertNewline"                 },
  { WebCore::VKEY_RETURN, AltKey | ShiftKey,  "InsertNewline"                 },
  { WebCore::VKEY_RETURN, ShiftKey,           "InsertLineBreak"               },
  { WebCore::VKEY_INSERT, CtrlKey,            "Copy"                          },	
  { WebCore::VKEY_INSERT, ShiftKey,           "Paste"                         },	
  { WebCore::VKEY_DELETE, ShiftKey,           "Cut"                           },	
#if defined(OS_MACOSX)
  { 'C',                  CommandKey,         "Copy"                          },
  { 'V',                  CommandKey,         "Paste"                         },
  { 'V',                  CommandKey | ShiftKey,
        "PasteAndMatchStyle"                                                  },
  { 'X',                  CommandKey,         "Cut"                           },
  { 'A',                  CommandKey,         "SelectAll"                     },
  { 'Z',                  CommandKey,         "Undo"                          },
  { 'Z',                  CommandKey | ShiftKey,
        "Redo"                                                                },
  { 'Y',                  CommandKey,         "Redo"                          },
#else
  { 'C',                  CtrlKey,            "Copy"                          },
  { 'V',                  CtrlKey,            "Paste"                         },
  { 'V',                  CtrlKey | ShiftKey, "PasteAndMatchStyle"            },
  { 'X',                  CtrlKey,            "Cut"                           },
  { 'A',                  CtrlKey,            "SelectAll"                     },
  { 'Z',                  CtrlKey,            "Undo"                          },
  { 'Z',                  CtrlKey | ShiftKey, "Redo"                          },
  { 'Y',                  CtrlKey,            "Redo"                          },
#endif
};

static const KeyPressEntry keyPressEntries[] = {
  { '\t',   0,                  "InsertTab"                                   },
  { '\t',   ShiftKey,           "InsertBacktab"                               },
  { '\r',   0,                  "InsertNewline"                               },
  { '\r',   CtrlKey,            "InsertNewline"                               },
  { '\r',   ShiftKey,           "InsertLineBreak"                             },
  { '\r',   AltKey,             "InsertNewline"                               },
  { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

const char* EditorClientImpl::interpretKeyEvent(
  const WebCore::KeyboardEvent* evt) {
  const WebCore::PlatformKeyboardEvent* keyEvent = evt->keyEvent();
  if (!keyEvent)
    return "";

  static HashMap<int, const char*>* keyDownCommandsMap = 0;
  static HashMap<int, const char*>* keyPressCommandsMap = 0;

  if (!keyDownCommandsMap) {
    keyDownCommandsMap = new HashMap<int, const char*>;
    keyPressCommandsMap = new HashMap<int, const char*>;

    for (unsigned i = 0; i < arraysize(keyDownEntries); i++) {
      keyDownCommandsMap->set(
        keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey,
        keyDownEntries[i].name);
    }

    for (unsigned i = 0; i < arraysize(keyPressEntries); i++) {
      keyPressCommandsMap->set(
        keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, 
        keyPressEntries[i].name);
    }
  }

  unsigned modifiers = 0;
  if (keyEvent->shiftKey())
    modifiers |= ShiftKey;
  if (keyEvent->altKey())
    modifiers |= AltKey;
  if (keyEvent->ctrlKey())
    modifiers |= CtrlKey;
  if (keyEvent->metaKey())
    modifiers |= MetaKey;

  if (keyEvent->type() == WebCore::PlatformKeyboardEvent::RawKeyDown) {
    int mapKey = modifiers << 16 | evt->keyCode();
    return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
  }

  int mapKey = modifiers << 16 | evt->charCode();
  return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool EditorClientImpl::handleEditingKeyboardEvent(
  WebCore::KeyboardEvent* evt) {
  const WebCore::PlatformKeyboardEvent* keyEvent = evt->keyEvent();
#if defined(OS_WIN)
  // do not treat this as text input if it's a system key event
  if (!keyEvent || keyEvent->isSystemKey())
      return false;
#endif

  WebCore::Frame* frame = evt->target()->toNode()->document()->frame();
  if (!frame)
    return false;

  const char* command_name = interpretKeyEvent(evt);
  WebCore::Editor::Command command = frame->editor()->command(command_name);

  if (keyEvent->type() == WebCore::PlatformKeyboardEvent::RawKeyDown) {
    // WebKit doesn't have enough information about mode to decide how
    // commands that just insert text if executed via Editor should be treated,
    // so we leave it upon WebCore to either handle them immediately
    // (e.g. Tab that changes focus) or let a keypress event be generated
    // (e.g. Tab that inserts a Tab character, or Enter).
    if (command.isTextInsertion() || !command_name)
      return false;
    if (command.execute(evt)) {
      WebViewDelegate* d = web_view_->delegate();
      MaybeRecordCommand(d, command_name);
      return true;
    }
    return false;
  }

  if (command.execute(evt)) {
    WebViewDelegate* d = web_view_->delegate();
    MaybeRecordCommand(d, command_name);
    return true;
  }

  if (evt->keyEvent()->text().length() == 1) {
    UChar ch = evt->keyEvent()->text()[0U];

    // Don't insert null or control characters as they can result in
    // unexpected behaviour
    if (ch < ' ')
      return false;
  }

  return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}

//
// End of code block subject to Apple, Inc. copyright
//

void EditorClientImpl::handleKeyboardEvent(WebCore::KeyboardEvent* evt) {
  if (handleEditingKeyboardEvent(evt))
    evt->setDefaultHandled();
}

void EditorClientImpl::handleInputMethodKeydown(WebCore::KeyboardEvent* keyEvent) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::textFieldDidBeginEditing(WebCore::Element*) {
}

void EditorClientImpl::textFieldDidEndEditing(WebCore::Element*) {
  // Notification that focus was lost.  Be careful with this, it's also sent
  // when the page is being closed.
}

void EditorClientImpl::textDidChangeInTextField(WebCore::Element* element) {
  // Track the element so we can blur/focus it in respondToChangedContents
  // so that the selected range is properly set. (See respondToChangedContents).
  if (static_cast<WebCore::HTMLInputElement*>(element)->autofilled())
    pending_inline_autocompleted_element_ = element;
}

bool EditorClientImpl::doTextFieldCommandFromEvent(WebCore::Element*,
                                                   WebCore::KeyboardEvent*) {
  // The Mac code appears to use this method as a hook to implement special
  // keyboard commands specific to Safari's auto-fill implementation.  We
  // just return false to allow the default action.
  return false;
}

void EditorClientImpl::textWillBeDeletedInTextField(WebCore::Element*) {
}

void EditorClientImpl::textDidChangeInTextArea(WebCore::Element*) {
}

void EditorClientImpl::ignoreWordInSpellDocument(const WebCore::String&) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::learnWord(const WebCore::String&) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::checkSpellingOfString(const UChar* str, int length,
                                             int* misspellingLocation,
                                             int* misspellingLength) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spell_location = -1;
  int spell_length = 0;
  WebViewDelegate* d = web_view_->delegate();
  if (web_view_->FocusedFrameNeedsSpellchecking() && d) {
    std::wstring word = 
        webkit_glue::StringToStdWString(WebCore::String(str, length));
    d->SpellCheck(word, spell_location, spell_length);
  } else {
    spell_location = 0;
    spell_length = 0;
  }

  // Note: the Mac code checks if the pointers are NULL before writing to them,
  // so we do too.
  if (misspellingLocation)
    *misspellingLocation = spell_location;
  if (misspellingLength)
    *misspellingLength = spell_length;
}

void EditorClientImpl::checkGrammarOfString(const UChar*, int length,
                                            WTF::Vector<WebCore::GrammarDetail>&,
                                            int* badGrammarLocation,
                                            int* badGrammarLength) {
  NOTIMPLEMENTED();
  if (badGrammarLocation)
    *badGrammarLocation = 0;
  if (badGrammarLength)
    *badGrammarLength = 0;
}

void EditorClientImpl::updateSpellingUIWithGrammarString(const WebCore::String&,
                                                         const WebCore::GrammarDetail& detail) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::updateSpellingUIWithMisspelledWord(const WebCore::String&) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::showSpellingUI(bool show) {
  NOTIMPLEMENTED();
}

bool EditorClientImpl::spellingUIIsShowing() {
  return false;
}

void EditorClientImpl::getGuessesForWord(const WebCore::String&,
                                         WTF::Vector<WebCore::String>& guesses) {
  NOTIMPLEMENTED();
}

void EditorClientImpl::setInputMethodState(bool enabled) {
  WebViewDelegate* d = web_view_->delegate();
  if (d) {
    d->SetInputMethodState(enabled);
  }
}


std::wstring EditorClientImpl::DescribeOrError(int number, 
                                               WebCore::ExceptionCode ec) {
  if (ec)
    return L"ERROR";

  return IntToWString(number);
}

std::wstring EditorClientImpl::DescribeOrError(WebCore::Node* node, 
                                               WebCore::ExceptionCode ec) {
  if (ec)
    return L"ERROR";

  return Describe(node);
}

// These Describe() functions match the output expected by the layout tests.
std::wstring EditorClientImpl::Describe(WebCore::Range* range) {
  if (range) {
    WebCore::ExceptionCode exception = 0;
    std::wstring str = L"range from ";
    int offset = range->startOffset(exception);
    str.append(DescribeOrError(offset, exception));
    str.append(L" of ");
    WebCore::Node* container = range->startContainer(exception);
    str.append(DescribeOrError(container, exception));
    str.append(L" to ");
    offset = range->endOffset(exception);
    str.append(DescribeOrError(offset, exception));
    str.append(L" of ");
    container = range->endContainer(exception);
    str.append(DescribeOrError(container, exception));
    return str;
  }
  return L"(null)";
}

// See comment for Describe(), above.
std::wstring EditorClientImpl::Describe(WebCore::Node* node) {
  if (node) {
    std::wstring str = webkit_glue::StringToStdWString(node->nodeName());
    WebCore::Node* parent = node->parentNode();
    if (parent) {
      str.append(L" > ");
      str.append(Describe(parent));
    }
    return str;
  }
  return L"(null)";
}

// See comment for Describe(), above.
std::wstring EditorClientImpl::Describe(WebCore::EditorInsertAction action) {
  switch (action) {
    case WebCore::EditorInsertActionTyped:
      return L"WebViewInsertActionTyped";
    case WebCore::EditorInsertActionPasted:
      return L"WebViewInsertActionPasted";
    case WebCore::EditorInsertActionDropped:
      return L"WebViewInsertActionDropped";
  }
  return L"(UNKNOWN ACTION)";
}

// See comment for Describe(), above.
std::wstring EditorClientImpl::Describe(WebCore::EAffinity affinity) {
  switch (affinity) {
    case WebCore::UPSTREAM:
      return L"NSSelectionAffinityUpstream";
    case WebCore::DOWNSTREAM:
      return L"NSSelectionAffinityDownstream";
  }
  return L"(UNKNOWN AFFINITY)";
}

std::wstring EditorClientImpl::Describe(WebCore::CSSStyleDeclaration* style) {
  // TODO(pamg): Implement me.  It's not clear what WebKit produces for this
  // (their [style description] method), and none of the layout tests provide
  // an example.  But because none of them use it, it's not yet important.
  return std::wstring();
}

