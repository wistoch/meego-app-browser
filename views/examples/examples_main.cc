// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/examples_main.h"

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "views/controls/label.h"
#include "views/controls/button/text_button.h"
#include "views/examples/button_example.h"
#include "views/examples/combobox_example.h"
#include "views/examples/message_box_example.h"
#include "views/examples/radio_button_example.h"
#include "views/examples/scroll_view_example.h"
#include "views/examples/tabbed_pane_example.h"
#include "views/examples/textfield_example.h"
#include "views/focus/accelerator_handler.h"
#include "views/grid_layout.h"
#include "views/window/window.h"

namespace examples {

views::View* ExamplesMain::GetContentsView() {
  return contents_;
}

void ExamplesMain::SetStatus(const std::wstring& status) {
  status_label_->SetText(status);
}

void ExamplesMain::Run() {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  app::RegisterPathProvider();

  icu_util::Initialize();

  // This requires chrome to be built first right now.
  // TODO(oshima): fix build to include resource file.
  ResourceBundle::InitSharedInstance(L"en-US");
  ResourceBundle::GetSharedInstance().LoadThemeResources();

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Creates a window with the tabbed pane for each examples, and
  // a label to print messages from each examples.
  DCHECK(contents_ == NULL) << "Run called more than once.";
  contents_ = new views::View();
  contents_->set_background(views::Background::CreateStandardPanelBackground());
  views::GridLayout* layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  views::TabbedPane* tabbed_pane = new views::TabbedPane();
  status_label_ = new views::Label();

  layout->StartRow(1, 0);
  layout->AddView(tabbed_pane);
  layout->StartRow(0 /* no expand */, 0);
  layout->AddView(status_label_);

  views::Window* window =
      views::Window::CreateChromeWindow(NULL, gfx::Rect(0, 0, 600, 300), this);

  examples::TextfieldExample textfield_example(this);
  tabbed_pane->AddTab(textfield_example.GetExampleTitle(),
                      textfield_example.GetExampleView());

  examples::ButtonExample button_example(this);
  tabbed_pane->AddTab(button_example.GetExampleTitle(),
                      button_example.GetExampleView());

  examples::ComboboxExample combobox_example(this);
  tabbed_pane->AddTab(combobox_example.GetExampleTitle(),
                      combobox_example.GetExampleView());

  examples::TabbedPaneExample tabbed_pane_example(this);
  tabbed_pane->AddTab(tabbed_pane_example.GetExampleTitle(),
                      tabbed_pane_example.GetExampleView());

  examples::MessageBoxExample message_box_example(this);
  tabbed_pane->AddTab(message_box_example.GetExampleTitle(),
                      message_box_example.GetExampleView());

  examples::RadioButtonExample radio_button_example(this);
  tabbed_pane->AddTab(radio_button_example.GetExampleTitle(),
                      radio_button_example.GetExampleView());

  examples::ScrollViewExample scroll_view_example(this);
  tabbed_pane->AddTab(scroll_view_example.GetExampleTitle(),
                      scroll_view_example.GetExampleView());

  window->Show();
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
}

}  // examples namespace

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);
#endif

  CommandLine::Init(argc, argv);
  examples::ExamplesMain main;
  main.Run();

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
