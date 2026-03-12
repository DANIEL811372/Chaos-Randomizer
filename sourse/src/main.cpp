#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

using namespace geode::prelude;

class $modify(MyEditorUI, EditorUI) {
    struct Fields {
        CCMenuItemSpriteExtra* m_randomBtn = nullptr;
        ButtonSprite* m_btnSprite = nullptr;
        std::set<int> m_excludedIDs;
        std::vector<int> m_safeIDs;
        bool m_isRandomEnabled = false;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        // Уведомление о сканировании
        if (auto notification = Notification::create("Scanning objects...", NotificationIcon::Loading)) {
            notification->show();
        }

        m_fields->m_safeIDs.clear();
        std::set<int> existingIDs;

        // Читаем настройки диапазона
        int minID = 1;
        int maxID = 3500;
        if (Mod::get()->hasSetting("min-id") && Mod::get()->hasSetting("max-id")) {
            minID = Mod::get()->getSettingValue<int64_t>("min-id");
            maxID = Mod::get()->getSettingValue<int64_t>("max-id");
        }
        minID = std::max(1, minID);
        maxID = std::max(minID, maxID);

        int tabsScanned = 0;
        int objectsFound = 0;

        if (!this->m_createButtonBars) {
            FLAlertLayer::create("Error", "Failed to access button bars!", "OK")->show();
            return true;
        }

        if (this->m_createButtonBars) {
            int totalTabs = this->m_createButtonBars->count();
            if (totalTabs == 0) {
                FLAlertLayer::create("Warning", "No tabs found to scan!", "OK")->show();
                return true;
            }

            for (int i = 0; i < totalTabs; ++i) {
                auto bar = static_cast<EditButtonBar*>(this->m_createButtonBars->objectAtIndex(i));
                if (!bar || !bar->m_buttonArray) continue;

                tabsScanned++;
                for (int j = 0; j < bar->m_buttonArray->count(); ++j) {
                    auto btn = static_cast<CreateMenuItem*>(bar->m_buttonArray->objectAtIndex(j));
                    if (btn && btn->m_objectID > 0) {
                        existingIDs.insert(btn->m_objectID);
                        objectsFound++;
                    }
                }
            }
        }

        // Формируем список безопасных ID (только существующие и не исключённые)
        for (int id : existingIDs) {
            if (id >= minID && id <= maxID) {
                if (m_fields->m_excludedIDs.count(id) == 0) {
                    m_fields->m_safeIDs.push_back(id);
                }
            }
        }

        if (m_fields->m_safeIDs.empty() && !existingIDs.empty()) {
            for (int id : existingIDs) {
                if (m_fields->m_excludedIDs.count(id) == 0) {
                    m_fields->m_safeIDs.push_back(id);
                }
            }
        }
        if (m_fields->m_safeIDs.empty()) {
            m_fields->m_safeIDs.push_back(1);
        }

        // Отчёт
        std::string reportMsg = "Scan complete!\n\n";
        reportMsg += "Requested range: " + std::to_string(minID) + " - " + std::to_string(maxID) + "\n";
        reportMsg += "Tabs scanned: " + std::to_string(tabsScanned) + "\n";
        reportMsg += "Total objects found: " + std::to_string(objectsFound) + "\n";
        reportMsg += "Unique objects: " + std::to_string(existingIDs.size()) + "\n";
        reportMsg += "Active in pool: " + std::to_string(m_fields->m_safeIDs.size()) + "\n\n";
        if (m_fields->m_safeIDs.size() < (maxID - minID + 1)) {
            reportMsg += "✓ Non-existent objects automatically excluded";
        }
        FLAlertLayer::create("Chaos Randomizer", reportMsg, "OK")->show();

        // --- ДОБАВЛЯЕМ КНОПКУ В ПРАВИЛЬНОЕ МЕНЮ ---
        auto winSize = CCDirector::get()->getWinSize();
        float desiredX = 80.f;      // отступ от левого края
        float desiredY = winSize.height - 90.f; // отступ от верха (90 пикселей)

        // Находим меню, которое точно скрывается в тестовом режиме
        CCMenu* targetMenu = nullptr;

        // 1. Пробуем найти меню отмены (обычно слева вверху)
        CCNode* undoMenu = this->getChildByID("undo-menu");
        if (undoMenu) {
            targetMenu = typeinfo_cast<CCMenu*>(undoMenu);
        }

        // 2. Если не нашли, пробуем найти любое дочернее меню
        if (!targetMenu) {
            auto children = this->getChildren();
            if (children) {
                for (int i = 0; i < children->count(); i++) {
                    auto node = children->objectAtIndex(i);
                    auto menu = typeinfo_cast<CCMenu*>(node);
                    if (menu && menu->isVisible()) {
                        targetMenu = menu;
                        break;
                    }
                }
            }
        }

        // 3. Если всё ещё не нашли, создаём своё (менее надёжно)
        if (!targetMenu) {
            log::warn("Could not find any menu, creating own");
            targetMenu = CCMenu::create();
            targetMenu->setPosition(0, 0);
            this->addChild(targetMenu, 100);
        }

        // Переводим желаемые глобальные координаты в локальные относительно меню
        CCPoint localPos = targetMenu->convertToNodeSpace({desiredX, desiredY});

        // Создаём кнопку
        m_fields->m_btnSprite = ButtonSprite::create("RND", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        m_fields->m_btnSprite->setScale(0.8f);

        auto btn = CCMenuItemSpriteExtra::create(
            m_fields->m_btnSprite,
            this,
            menu_selector(MyEditorUI::onToggleRandom)
        );
        btn->setTag(0);
        m_fields->m_randomBtn = btn;
        btn->setPosition(localPos);

        targetMenu->addChild(btn);
        m_fields->m_btnSprite->setColor({180, 180, 180});

        return true;
    }

    void onToggleRandom(CCObject* sender) {
        m_fields->m_isRandomEnabled = !m_fields->m_isRandomEnabled;

        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        btn->setTag(m_fields->m_isRandomEnabled ? 1 : 0);

        if (m_fields->m_isRandomEnabled) {
            m_fields->m_btnSprite->setColor({0, 255, 0});
        } else {
            m_fields->m_btnSprite->setColor({180, 180, 180});
        }

        std::string msg = m_fields->m_isRandomEnabled ? "Random Mode: ON" : "Random Mode: OFF";
        auto icon = m_fields->m_isRandomEnabled ? NotificationIcon::Success : NotificationIcon::Error;
        Notification::create(msg.c_str(), icon)->show();

        if (m_fields->m_isRandomEnabled && !m_fields->m_safeIDs.empty()) {
            this->getCreateBtn(1, 1)->activate();
        }
    }

    void onCreateObject(int id) {
        if (m_fields->m_isRandomEnabled && !m_fields->m_safeIDs.empty()) {
            int randomIndex = std::rand() % m_fields->m_safeIDs.size();
            int newID = m_fields->m_safeIDs[randomIndex];
            log::info("Randomized: {} -> {}", id, newID);
            EditorUI::onCreateObject(newID);
        } else {
            EditorUI::onCreateObject(id);
        }
    }

    void onExcludeObject(CCObject*) {
        auto selectedObjects = this->getSelectedObjects();
        if (selectedObjects && selectedObjects->count() > 0) {
            if (auto obj = static_cast<GameObject*>(selectedObjects->objectAtIndex(0))) {
                int objID = obj->m_objectID;
                m_fields->m_excludedIDs.insert(objID);

                m_fields->m_safeIDs.clear();

                std::set<int> existingIDs;
                if (this->m_createButtonBars) {
                    int totalTabs = this->m_createButtonBars->count();
                    for (int i = 0; i < totalTabs; ++i) {
                        auto bar = static_cast<EditButtonBar*>(this->m_createButtonBars->objectAtIndex(i));
                        if (!bar || !bar->m_buttonArray) continue;
                        for (int j = 0; j < bar->m_buttonArray->count(); ++j) {
                            auto btn = static_cast<CreateMenuItem*>(bar->m_buttonArray->objectAtIndex(j));
                            if (btn && btn->m_objectID > 0) {
                                existingIDs.insert(btn->m_objectID);
                            }
                        }
                    }
                }

                int minID = Mod::get()->getSettingValue<int64_t>("min-id");
                int maxID = Mod::get()->getSettingValue<int64_t>("max-id");

                for (int id : existingIDs) {
                    if (id >= minID && id <= maxID) {
                        if (m_fields->m_excludedIDs.count(id) == 0) {
                            m_fields->m_safeIDs.push_back(id);
                        }
                    }
                }

                if (m_fields->m_safeIDs.empty()) m_fields->m_safeIDs.push_back(1);

                Notification::create(("Object " + std::to_string(objID) + " excluded").c_str(),
                                   NotificationIcon::Info)->show();
            }
        }
    }
};