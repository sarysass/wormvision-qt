#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

class ThemeManager : public QObject {
  Q_OBJECT
public:
  static ThemeManager &instance();

  void applyTheme(const QString &themeName);
  QString currentTheme() const { return m_currentTheme; }

private:
  explicit ThemeManager(QObject *parent = nullptr);
  QString m_currentTheme;
};

#endif // THEMEMANAGER_H
