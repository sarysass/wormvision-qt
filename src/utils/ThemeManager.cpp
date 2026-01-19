#include "ThemeManager.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>

ThemeManager &ThemeManager::instance() {
  static ThemeManager instance;
  return instance;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

void ThemeManager::applyTheme(const QString &themeName) {
  QString qssPath;
  if (themeName == "dark") {
    // Try resource first, then filesystem
    if (QFile::exists(":/resources/styles/dark.qss")) {
      qssPath = ":/resources/styles/dark.qss";
    } else {
      // Dev mode fallback
      qssPath = "resources/styles/dark.qss";
    }
  } else {
    // Light or Default
    // Light Theme
    if (QFile::exists(":/resources/styles/light.qss")) {
      qssPath = ":/resources/styles/light.qss";
    } else {
      qssPath = "resources/styles/light.qss";
    }
  }
  QFile file(qssPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    QString styleSheet = stream.readAll();
    qApp->setStyleSheet(styleSheet);
    m_currentTheme = themeName;
    qDebug() << "应用主题:" << themeName;
  } else {
    qWarning() << "加载主题失败:" << qssPath;
  }
}
