// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webpopupwindow.h"
#include "webpage.h"
#include "webview.h"
#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QLineEdit>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

WebPopupWindow::WebPopupWindow(QWebEngineProfile *profile,
                               const QRect &geometry, QWidget *parent)
    : m_favAction(new QAction(this)), m_view(new WebView(profile, this)),
      m_initialGeometry(geometry) {
  setAttribute(Qt::WA_DeleteOnClose);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  // Set initial geometry before showing
  if (m_initialGeometry.isValid() && m_initialGeometry.width() > 30 &&
      m_initialGeometry.height() > 30) {
    move(m_initialGeometry.topLeft());
    resize(m_initialGeometry.size());
  } else {
    // Default size for popups that don't specify geometry
    resize(400, 600);

    QScreen *screen = nullptr;
    if (parent) {
      screen = parent->screen();
    }
    if (!screen) {
      screen = QGuiApplication::primaryScreen();
    }

    // Center on screen on parent
    if (screen) {
      QRect screenGeometry = screen->availableGeometry();
      move(screenGeometry.center() - rect().center());
    }
  }

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
  if (!newGeometry.isValid()) {
    show();
    m_view->setFocus();
    return;
  }

  // Ensure we have a window handle by calling create() or show()/hide()
  if (!windowHandle()) {
    // Create the native window without showing it
    create();
  }

  if (QWindow *window = windowHandle()) {
    setGeometry(newGeometry.marginsRemoved(window->frameMargins()));
  } else {
    // Fallback if still no window handle
    move(newGeometry.topLeft());
    resize(newGeometry.size());
  }

  show();
  m_view->setFocus();
}
