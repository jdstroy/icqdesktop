#include "stdafx.h"

#include "ContactListItemDelegate.h"
#include "../../cache/avatars/AvatarStorage.h"

#include "SearchModel.h"
#include "SearchMembersModel.h"
#include "../../types/contact.h"
#include "../../utils/profiling/auto_stop_watch.h"
#include "../../utils/utils.h"
#include "ContactListItemRenderer.h"
#include "ContactListModel.h"
#include "ChatMembersModel.h"
#include "ContactList.h"

namespace Logic
{
    ContactListItemDelegate::ContactListItemDelegate(QObject* parent, int _regim)
        : QItemDelegate(parent)
        , StateBlocked_(false)
        , regim_(_regim)
        , width_(-1)
        , leftMargin_(0)
        , rightMargin_(0)
        , renderRole_(false)
    {
    }

    ContactListItemDelegate::~ContactListItemDelegate()
    {
    }

    void ContactListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        const auto searchMemberModel = qobject_cast<const Logic::SearchMembersModel*>(index.model());
        const auto membersModel = qobject_cast<const Logic::ChatMembersModel*>(index.model());
        
        bool isGroup = false;
        QString displayName, status, state;
        QString aimId;
        bool haveLastSeen = false, isChecked = false, isChatMember = false, isOfficial = false;
        QDateTime lastSeen;
        
        bool hasMouseOver = true;
        if (platform::is_apple())
        {
            if (const auto customModel = qobject_cast<const Logic::CustomAbstractListModel *>(index.model()))
                hasMouseOver = customModel->customFlagIsSet(Logic::CustomAbstractListModelFlags::HasMouseOver);
        }

        Data::Contact* contact_in_cl = NULL;
        QString role;

        if ((membersModel || searchMemberModel))
        {
            auto cont = index.data(Qt::DisplayRole).value<Data::ChatMemberInfo*>();
            isGroup = false;

            aimId = cont->AimId_;

            auto contact_item = Logic::GetContactListModel()->getContactItem(aimId);
            contact_in_cl = contact_item == NULL ? NULL : contact_item->Get();
            
            displayName = cont->NickName_.isEmpty()
                ? (cont->FirstName_.isEmpty() && cont->LastName_.isEmpty() 
                    ? cont->AimId_
                    : cont->FirstName_ + " " + cont->LastName_)
                : cont->NickName_;
    
            status = QString();
            state = QString();
            if (renderRole_)
                role = cont->Role_;
        }

        if ((!membersModel && !searchMemberModel) || contact_in_cl)
        {
            if (!membersModel && !searchMemberModel)
                contact_in_cl = index.data(Qt::DisplayRole).value<Data::Contact*>();
            isGroup = (contact_in_cl->GetType() == Data::GROUP);
            displayName = contact_in_cl->GetDisplayName();
            aimId = contact_in_cl->AimId_;
            status = contact_in_cl->StatusMsg_;
            state = contact_in_cl->State_;
            haveLastSeen = contact_in_cl->HaveLastSeen_;
            lastSeen = contact_in_cl->LastSeen_;
            isChecked = contact_in_cl->IsChecked_;
            isOfficial = contact_in_cl->IsOfficial_;
            
            auto chatMembersModel = Logic::GetChatMembersModel();
            if (!!chatMembersModel)
            {
                isChatMember = chatMembersModel->is_contact_in_chat(aimId);
            }
        }

        const bool isSelected = ((option.state & QStyle::State_Selected) && !isGroup) && !StateBlocked_;
        const bool isHovered =  ((option.state & QStyle::State_MouseOver) && !isGroup) && !StateBlocked_ && !isSelected && hasMouseOver;
        
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(option.rect.topLeft());

        if (isGroup)
        {
            ContactList::RenderGroupItem(*painter, displayName, leftMargin_);
        }
        else
        {
            const auto isMultichat = Logic::GetContactListModel()->isChat(aimId);

            const auto isFilled = !isMultichat;
            bool isDefault = false;

            const auto &avatar = Logic::GetAvatarStorage()->GetRounded(aimId, QString(), Utils::scale_bitmap(Utils::scale_value(32)) , isMultichat ? QString() : state, isFilled, isDefault, false, true);
            const ContactList::ContactListVisualData visData(aimId, *avatar, state, status, isHovered, isSelected, displayName, haveLastSeen, lastSeen, isChecked, isChatMember, isOfficial, width_, leftMargin_, rightMargin_, role);
            ContactList::RenderContactItem(*painter, visData, regim_, membersModel && membersModel->is_short_view_);
        }

        if (index == DragIndex_)
            ContactList::RenderContactsDragOverlay(*painter);

        painter->restore();
    }

    QSize ContactListItemDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex &index) const
    {
        const auto membersModel = qobject_cast<const Logic::ChatMembersModel*>(index.model());
        int width = width_ == -1 ? Utils::scale_value(333) : width_;
        if (!membersModel)
        {
            auto cont = index.data(Qt::DisplayRole).value<Data::Contact*>();

            if (!cont)
                return QSize(width, ContactList::ContactItemHeight());

            const auto isGroup = (cont->GetType() == Data::GROUP);
            if (isGroup)
            {
                return QSize(width, Utils::scale_value(28));
            }
        }
        else
        {
            if (membersModel->is_short_view_)
                return QSize(width, ContactList::ContactItemHeight());
            else
                return QSize(width, ContactList::ContactItemHeight());
        }
        return QSize(width, ContactList::ContactItemHeight());
    }

    void ContactListItemDelegate::blockState(bool value)
    {
        StateBlocked_= value;
    }

    void ContactListItemDelegate::setDragIndex(const QModelIndex& index)
    {
        DragIndex_ = index;
    }

    void ContactListItemDelegate::setItemWidth(int width)
    {
        width_ = width;
    }

    void ContactListItemDelegate::setLeftMargin(int margin)
    {
        leftMargin_ = margin;
    }

    void ContactListItemDelegate::setRightMargin(int margin)
    {
        rightMargin_ = margin;
    }

    void ContactListItemDelegate::setRegim(int _regim)
    {
        regim_ = _regim;
    }

    void ContactListItemDelegate::setRenderRole(bool render)
    {
        renderRole_ = render;
    }
}