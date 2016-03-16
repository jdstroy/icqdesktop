#ifndef ThemesWidget_h
#define ThemesWidget_h
#include "../../../cache/themes/themes.h"

namespace Ui {
    class FlowLayout;
    class ThemesModel;
    
    class ThemesWidget : public QWidget
    {
        Q_OBJECT
        FlowLayout *flowLayout_;
        ThemesModel *themesModel_;
        themes::themes_list themes_list_;
        QMap<int,bool> loaded_themes_;
    public:
        ThemesWidget(QWidget* _parent, int _spacing);
        void onThemeGot(themes::themePtr theme);
        void set_target_contact(QString _aimId);
    };

}
#endif /* ThemesTableWidget_h */
