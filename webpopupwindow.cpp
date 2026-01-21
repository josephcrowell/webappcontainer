// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webpopupwindow.h"
#include "webpage.h"
#include "webview.h"
#include <QAction>
#include <QIcon>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWindow>

WebPopupWindow::WebPopupWindow(QWebEngineProfile *profile)
    : m_urlLineEdit(new QLineEdit(this)), m_favAction(new QAction(this)),
      m_view(new WebView(profile, this)) {
  setAttribute(Qt::WA_DeleteOnClose);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  QVBoxLayout *layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  layout->addWidget(m_view);

  m_view->setPage(new WebPage(profile, m_view));
  m_view->setFocus();

  connect(m_view, &WebView::titleChanged, this, &QWidget::setWindowTitle);
  connect(m_view, &WebView::favIconChanged, m_favAction, &QAction::setIcon);
  connect(m_view->page(), &WebPage::geometryChangeRequested, this,
          &WebPopupWindow::handleGeometryChangeRequested);
  connect(m_view->page(), &WebPage::windowCloseRequested, this,
          &QWidget::close);
}

WebView *WebPopupWindow::view() const { return m_view; }

void WebPopupWindow::handleGeometryChangeRequested(const QRect &newGeometry) {
  if (QWindow *window = windowHandle())
    setGeometry(newGeometry.marginsRemoved(window->frameMargins()));
  show();
  m_view->setFocus();
}
