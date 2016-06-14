#include "stdafx.h"

#include "core_dispatcher.h"

#include "utils/InterConnector.h"
#include "utils/log/log.h"
#include "utils/profiling/auto_stop_watch.h"
#include "utils/utils.h"
#include "utils/uid.h"
#include "utils/LoadPixmapFromDataTask.h"

#include "../corelib/collection_helper.h"
#include "../corelib/enumerations.h"

#include "utils/gui_coll_helper.h"

#include "cache/stickers/stickers.h"
#include "cache/themes/themes.h"
#include "../core/Voip/VoipManagerDefines.h"
#include "../core/connections/wim/loader/loader_errors.h"
#include "gui_settings.h"
#include "app_config.h"
#include "my_info.h"
#include "../common.shared/crash_handler.h"
#include "main_window/contact_list/SearchMembersModel.h"
#include "types/typing.h"
#include "../corelib/corelib.h"

#include "theme_settings.h"

using namespace Ui;


int Ui::gui_connector::addref()
{
    return ++ref_count_;
}

int Ui::gui_connector::release()
{
    if (0 == (--ref_count_))
    {
        delete this;
        return 0;
    }

    return ref_count_;
}

void Ui::gui_connector::link( iconnector* )
{

}

void Ui::gui_connector::unlink()
{

}

void Ui::gui_connector::receive( const char * _message, int64_t _seq, core::icollection* _message_data)
{
    if (_message_data)
        _message_data->addref();

    emit received(QString(_message), _seq, _message_data);
}




core_dispatcher::core_dispatcher()
    : core_connector_(nullptr)
    , core_face_(nullptr)
    , gui_connector_(nullptr)
    , _voip_controller(*this)
    , last_time_callbacks_cleaned_up_(QDateTime::currentDateTimeUtc())
    , is_stats_enabled_(true)
    , is_im_created_(false)
{
    init();
}


core_dispatcher::~core_dispatcher()
{
    uninit();
}

voip_proxy::VoipController& core_dispatcher::getVoipController()
{
    return _voip_controller;
}

qint64 core_dispatcher::downloadSharedFile(const QString &contact, const QString &url, const QString &download_dir, const QString& filename, const core::file_sharing_function _function)
{
    assert(!contact.isEmpty());
    assert(!url.isEmpty());
    assert(!download_dir.isEmpty());
    assert(_function > core::file_sharing_function::min);
    assert(_function < core::file_sharing_function::max);

    QDir().mkpath(download_dir); // just in case

    core::coll_helper helper(create_collection(), true);
    helper.set<QString>("contact", contact);
    helper.set<QString>("url", url);
    helper.set<QString>("download_dir", download_dir);
    helper.set<QString>("filename", filename);
    helper.set<core::file_sharing_function>("function", _function);

    return post_message_to_core("files/download", helper.get());
}

qint64 core_dispatcher::abortSharedFileDownloading(const QString &contact, const QString &url, const qint64 downloadingSeq)
{
    assert(!contact.isEmpty());
    assert(!url.isEmpty());
    assert(downloadingSeq > 0);

    core::coll_helper helper(create_collection(), true);
    helper.set_value_as_string("contact", contact.toStdString());
    helper.set_value_as_string("url", url.toStdString());
    helper.set_value_as_int64("process_seq", downloadingSeq);

    return post_message_to_core("files/download/abort", helper.get());
}

qint64 core_dispatcher::uploadSharedFile(const QString &contact, const QString &localPath)
{
    core::coll_helper collection(create_collection(), true);
    collection.set_value_as_string("contact", contact.toUtf8().data());
    collection.set_value_as_string("file", localPath.toUtf8().data());

    return post_message_to_core("files/upload", collection.get());
}

qint64 core_dispatcher::abortSharedFileUploading(const QString &contact, const QString &localPath, const QString &uploadingProcessId)
{
    assert(!contact.isEmpty());
    assert(!localPath.isEmpty());
    assert(!uploadingProcessId.isEmpty());

    core::coll_helper helper(create_collection(), true);
    helper.set_value_as_string("contact", contact.toStdString());
    helper.set_value_as_string("local_path", localPath.toStdString());
    helper.set_value_as_string("process_seq", uploadingProcessId.toStdString());

    return post_message_to_core("files/upload/abort", helper.get());
}

qint64 core_dispatcher::getSticker(const qint32 setId, const qint32 stickerId, const core::sticker_size size)
{
    assert(setId > 0);
    assert(stickerId > 0);
    assert(size > core::sticker_size::min);
    assert(size < core::sticker_size::max);

    core::coll_helper collection(create_collection(), true);
    collection.set_value_as_int("set_id", setId);
    collection.set_value_as_int("sticker_id", stickerId);
    collection.set_value_as_enum("size", size);

    return post_message_to_core("stickers/sticker/get", collection.get());
}

qint64 core_dispatcher::getTheme(const qint32 _themeId)
{
    assert(_themeId >= 0);

    core::coll_helper collection(create_collection(), true);
    collection.set_value_as_int("theme_id", _themeId);

    return post_message_to_core("themes/theme/get", collection.get());
}

int64_t core_dispatcher::downloadImage(const QUrl &uri, const QString& destination, const bool isPreview)
{
    assert(uri.isValid());
    assert(!uri.isLocalFile());
    assert(!uri.isRelative());

    core::coll_helper collection(create_collection(), true);
    collection.set<QUrl>("uri", uri);
    collection.set<QString>("destination", destination);
    collection.set<bool>("is_preview", isPreview);

    const int32_t previewHeight = (isPreview ? (int32_t)Utils::scale_value(240) : 0);
    collection.set<int32_t>("preview_height", previewHeight);

    return post_message_to_core("image/download", collection.get());
}

qint64 core_dispatcher::delete_messages(const std::vector<int64_t> &_message_ids, const QString &_contact_aimid, const bool _for_all)
{
    assert(!_message_ids.empty());
    assert(!_contact_aimid.isEmpty());

    core::coll_helper params(create_collection(), true);

    core::ifptr<core::iarray> messages_ids(params->create_array());

    for (const auto id : _message_ids)
    {
        assert(id > 0);

        core::ifptr<core::ivalue> val(params->create_value());
        val->set_as_int64(id);
        messages_ids->push_back(val.get());
    }

    params.set_value_as_array("messages_ids", messages_ids.get());

    params.set<QString>("contact_aimid", _contact_aimid);
    params.set<bool>("for_all", _for_all);

    return post_message_to_core("archive/messages/delete", params.get());
}

qint64 core_dispatcher::delete_messages_from(const QString &_contact_aimid, const int64_t _from_id)
{
    assert(!_contact_aimid.isEmpty());
    assert(_from_id >= 0);

    core::coll_helper collection(create_collection(), true);

    collection.set<QString>("contact_aimid", _contact_aimid);
    collection.set<int64_t>("from_id", _from_id);

    return post_message_to_core("archive/messages/delete_from", collection.get());
}

bool core_dispatcher::init()
{
#ifndef __linux__
    QLibrary libcore(CORELIBRARY);
    if (!libcore.load())
    {
        assert(false);
        return false;
    }

    typedef bool (*get_core_instance_function)(core::icore_interface**);
    get_core_instance_function get_core_instance = (get_core_instance_function) libcore.resolve("get_core_instance");

    core::icore_interface* core_face = nullptr;
    if (!get_core_instance(&core_face))
    {
        assert(false);
        return false;
    }

    core_face_ = core_face;
#else
    core::icore_interface* core_face = nullptr;
    if (!get_core_instance(&core_face) || !core_face)
    {
        assert(false);
        return false;
    }
    core_face_ = core_face;
#endif //__linux__
    core_connector_ = core_face_->get_core_connector();
    if (!core_connector_)
        return false;

    gui_connector* connector = new gui_connector();
    QObject::connect(connector, SIGNAL(received(QString, qint64, core::icollection*)), this, SLOT(received(QString, qint64, core::icollection*)), Qt::QueuedConnection);

    gui_connector_ = connector;
    core_connector_->link(gui_connector_);

    return true;
}

void core_dispatcher::uninit()
{
    if (gui_connector_)
    {
        core_connector_->unlink();
        gui_connector_->release();

        gui_connector_ = nullptr;
    }

    if (core_connector_)
    {
        core_connector_->release();
        core_connector_ = nullptr;
    }

    if (core_face_)
    {
        core_face_->release();
        core_face_ = nullptr;
    }

}

void core_dispatcher::execute_callback(const int64_t seq, core::icollection* params)
{
    assert(seq > 0);

    const auto callback_info_iter = callbacks_.find(seq);
    if (callback_info_iter == callbacks_.end())
    {
        return;
    }

    const auto &callback = std::get<0>(callback_info_iter->second);

    assert(callback);
    callback(params);

    callbacks_.erase(callback_info_iter);
}

void core_dispatcher::cleanup_callbacks()
{
    const auto now = QDateTime::currentDateTimeUtc();

    const auto secs_passed_from_last_cleanup = last_time_callbacks_cleaned_up_.secsTo(now);
    const auto clean_timeout_in_secs = 30;
    if (secs_passed_from_last_cleanup < clean_timeout_in_secs)
    {
        return;
    }

    for (auto pairIter = callbacks_.begin(); pairIter != callbacks_.end();)
    {
        const auto callbackInfo = pairIter->second;

        const auto callbackRegisteredTimestamp = std::get<1>(callbackInfo);

        const auto callbackAgeInSeconds = callbackRegisteredTimestamp.secsTo(now);

        const auto callbackAgeInSecondsMax = 60;
        if (callbackAgeInSeconds > callbackAgeInSecondsMax)
        {
            pairIter = callbacks_.erase(pairIter);
        }
        else
        {
            ++pairIter;
        }
    }

    last_time_callbacks_cleaned_up_ = now;
}

void core_dispatcher::fileSharingDownloadResult(const int64_t seq, core::coll_helper _params)
{
    const auto function = _params.get_value_as_enum<core::file_sharing_function>("function");

    const auto mode_download_preview_metainfo = (function == core::file_sharing_function::download_preview_metainfo);
    const auto mode_check_local_copy_exists = (function == core::file_sharing_function::check_local_copy_exists);
    const auto mode_download_file = (function == core::file_sharing_function::download_file);
    const auto mode_download_file_metainfo = (function == core::file_sharing_function::download_file_metainfo);

    const auto raw_uri = _params.get<QString>("file_url");
    const auto error_code = _params.get<int32_t>("error", 0);

    const auto request_failed = (error_code != 0);
    if ((mode_download_file || mode_download_file_metainfo || mode_download_preview_metainfo) &&
        request_failed)
    {
        emit fileSharingError(seq, raw_uri, error_code);
        return;
    }

    if (mode_download_preview_metainfo)
    {
        const auto miniPreviewUri = _params.get<QString>("mini_preview_uri");
        const auto fullPreviewUri = _params.get<QString>("full_preview_uri");

        emit fileSharingPreviewMetainfoDownloaded(seq, miniPreviewUri, fullPreviewUri);

        return;
    }

    const auto filename = _params.get<QString>("file_name");
    const auto filename_short = _params.get<QString>("file_name_short");
    const auto size = _params.get<int64_t>("file_size");
    const auto download_uri = _params.get<QString>("file_dlink");
    const auto bytes_transfer = _params.get<int64_t>("bytes_transfer", 0);

    const auto is_progress = !_params.is_value_exist("error");
    if (is_progress)
    {
        assert(bytes_transfer >= 0);
        assert(mode_download_file);
        emit fileSharingFileDownloading(seq, raw_uri, bytes_transfer);
        return;
    }

    if (mode_download_file)
    {
        emit fileSharingFileDownloaded(seq, raw_uri, filename);
        return;
    }

    if (mode_download_file_metainfo)
    {
        emit fileSharingFileMetainfoDownloaded(seq, filename_short, download_uri, size);
        return;
    }

    mode_check_local_copy_exists;
    assert(mode_check_local_copy_exists);

    emit fileSharingLocalCopyCheckCompleted(seq, !request_failed, filename);
}

void core_dispatcher::imageDownloadResult(const int64_t seq, core::coll_helper _params)
{
    const auto is_progress = !_params.is_value_exist("error");
    if (is_progress)
    {
        return;
    }

    const auto rawUri = _params.get_value_as_string("url");
    const auto data = _params.get_value_as_stream("data");
    const auto local = _params.get_value_as_string("local");

    if (data->empty())
    {
        emit imageDownloadError(seq, rawUri);
        return;
    }

    auto task = new Utils::LoadPixmapFromDataTask(seq, rawUri, data, local);

    const auto succeeded = QObject::connect(
        task, &Utils::LoadPixmapFromDataTask::loadedSignal,
        this, &core_dispatcher::imageDownloaded
    );
    assert(succeeded);

    QThreadPool::globalInstance()->start(task);
}

void core_dispatcher::fileUploadingProgress(core::coll_helper _params)
{
    const auto bytesUploaded = _params.get<int64_t>("bytes_transfer");
    const auto uploadingId = _params.get<QString>("uploading_id");

    emit fileSharingUploadingProgress(uploadingId, bytesUploaded);
}

void core_dispatcher::fileUploadingResult(core::coll_helper _params)
{
    const auto uploadingId = _params.get<QString>("uploading_id");
    const auto link = _params.get<QString>("file_url");
    const auto error = _params.get<int32_t>("error");

    const auto success = (error == 0);
    const auto isFileTooBig = (error == core::wim::loader_errors::too_large_file);
    emit fileSharingUploadingResult(uploadingId, success, link, isFileTooBig);
}

core::icollection* core_dispatcher::create_collection() const
{
    core::ifptr<core::icore_factory> factory(core_face_->get_factory());
    return factory->create_collection();
}

qint64 core_dispatcher::post_message_to_core(const QString& message, core::icollection *collection, const message_processed_callback callback)
{
    const auto seq = Utils::get_uid();

    core_connector_->receive(message.toUtf8(), seq, collection);

    if (callback)
    {
        const auto result = callbacks_.emplace(seq, std::make_tuple(callback, QDateTime::currentDateTimeUtc()));
        assert(result.second);
    }

    return seq;
}

void core_dispatcher::set_enabled_stats_post(bool _is_stats_enabled)
{
    is_stats_enabled_ = _is_stats_enabled;
}

bool core_dispatcher::get_enabled_stats_post() const
{
    return is_stats_enabled_;
}

qint64 core_dispatcher::post_stats_to_core(core::stats::stats_event_names event_name)
{
    core::stats::event_props_type props;
    return post_stats_to_core(event_name, props);
}

qint64 core_dispatcher::post_stats_to_core(core::stats::stats_event_names event_name, const core::stats::event_props_type& _props)
{
    if (!get_enabled_stats_post())
        return -1;

    assert(event_name > core::stats::stats_event_names::min);
    assert(event_name < core::stats::stats_event_names::max);

    core::coll_helper coll(create_collection(), true);
    coll.set_value_as_enum("event", event_name);

    core::ifptr<core::iarray> props_array(coll->create_array());

    for (const auto &prop : _props)
    {
        assert(!prop.first.empty() && !prop.second.empty());

        core::coll_helper _coll_prop(coll->create_collection(), true);

        _coll_prop.set_value_as_string("name", prop.first);
        _coll_prop.set_value_as_string("value", prop.second);
        core::ifptr<core::ivalue> val(coll->create_value());
        val->set_as_collection(_coll_prop.get());
        props_array->push_back(val.get());
    }

    coll.set_value_as_array("props", props_array.get());
    return post_message_to_core("stats", coll.get());
}

void core_dispatcher::received(const QString received_message, const qint64 seq, core::icollection* params)
{
    if (seq > 0)
    {
        execute_callback(seq, params);
    }

    cleanup_callbacks();

    core::coll_helper coll_params(params, true);

    if (received_message == "need_login")
    {
        emit needLogin();
    }
    else if (received_message == "im/created")
    {
        is_im_created_ = true;
        
        emit im_created();
    }
    else if (received_message == "login/complete")
    {
        emit login_complete();
    }
    else if (received_message == "contactlist" ||
        received_message == "contactlist/diff")
    {
        auto cl = std::make_shared<Data::ContactList>();
        QString type;
        Data::UnserializeContactList(&coll_params, *cl, type);
        emit contactList(cl, type);
    }
    else if (received_message == "login_get_sms_code_result")
    {
        bool result = coll_params.get_value_as_bool("result");
        int code_length = coll_params.get_value_as_int("code_length", 0);
        int error = result ? 0 : coll_params.get_value_as_int("error");
        emit getSmsResult(seq, error, code_length);
    }
    else if (received_message == "login_result")
    {
        bool result = coll_params.get_value_as_bool("result");
        int error = result ? 0 : coll_params.get_value_as_int("error");
        emit loginResult(seq, error);
    }
    else if (received_message == "avatars/get/result")
    {
        std::unique_ptr<QPixmap> avatar(Data::UnserializeAvatar(&coll_params));
        const bool convert = coll_params.get_value_as_bool("avatar_need_to_convert");
        if (convert) {
            _voip_controller.setAvatars(*avatar.get(), coll_params.get_value_as_int("size"), coll_params.get_value_as_string("contact"));
        }

        const QString contact(coll_params.get_value_as_string("contact"));
        const int size = coll_params.get_value_as_int("size");
        emit avatarLoaded(contact, avatar.release(), size);
    }
    else if (received_message == "contact_presence")
    {
        emit presense(Data::UnserializePresence(&coll_params));
    }
    else if (received_message == "gui_settings")
    {
#ifdef _WIN32
        auto data_path = coll_params.get_value_as_string("data_path");
        core::dump::set_os_version(coll_params.get_value_as_string("os_version"));
#endif

        get_gui_settings()->unserialize(coll_params);
        emit guiSettings();
    }
    else if (received_message == "theme_settings")
    {
        get_qt_theme_settings()->unserialize(coll_params);
        emit themeSettings();
    }
    else if (received_message == "search_result")
    {
        QStringList contacts;
        Data::UnserializeSearchResult(&coll_params, contacts);
        emit searchResult(contacts);
    }
    else if (
        received_message == "archive/messages/get/result" ||
        received_message == "messages/received/dlg_state" ||
        received_message == "messages/received/server"	  ||
        received_message == "archive/messages/pending"    ||
        received_message == "messages/received/init")
    {
        const auto myAimid = coll_params.get<QString>("my_aimid");

        QString aimId;
        bool havePending = false;
        auto msgs = std::make_shared<Data::MessageBuddies>();
        auto modifications = std::make_shared<Data::MessageBuddies>();
        Data::UnserializeMessageBuddies(&coll_params, myAimid, Out aimId, Out havePending, Out *msgs, Out *modifications);

        auto type = Ui::MessagesBuddiesOpt::Requested;
        if (received_message == "messages/received/dlg_state")
        {
            type = Ui::MessagesBuddiesOpt::DlgState;
        }
        else if (received_message == "messages/received/server")
        {
            type = Ui::MessagesBuddiesOpt::FromServer;
        }
        else if (received_message == "archive/messages/pending")
        {
            type = Ui::MessagesBuddiesOpt::Pending;
        }
        else if (received_message == "messages/received/init")
        {
            type = Ui::MessagesBuddiesOpt::Init;
        }
        else if (received_message == "messages/received/message_status")
        {
            type = Ui::MessagesBuddiesOpt::MessageStatus;
        }

        emit messageBuddies(msgs, aimId, type, havePending, seq);

        if (coll_params.is_value_exist("deleted"))
        {
            QList<int64_t> deletedIds;

            const auto deletedIdsArray = coll_params.get_value_as_array("deleted");
            assert(!deletedIdsArray->empty());

            for (auto i = 0; i < deletedIdsArray->size(); ++i)
            {
                deletedIds.push_back(deletedIdsArray->get_at(i)->get_as_int64());
            }

            emit messagesDeleted(aimId, deletedIds);
        }

        if (!modifications->empty())
        {
            emit messagesModified(aimId, modifications);
        }
    }
    else if (received_message == "messages/del_up_to")
    {
        const auto id = coll_params.get<int64_t>("id");
        assert(id > -1);

        const auto contact = coll_params.get<QString>("contact");
        assert(!contact.isEmpty());

        emit messagesDeletedUpTo(contact, id);
    }
    else if (received_message == "dlg_state")
    {
        const auto myAimid = coll_params.get<QString>("my_aimid");

        Data::DlgState state;
        Data::UnserializeDlgState(&coll_params, myAimid, Out state);
        emit dlgState(state);
    }
    else if(received_message =="voip_signal")
    {
        _voip_controller.handlePacket(coll_params);
    }
    else if (received_message == "active_dialogs_are_empty")
    {
        emit Utils::InterConnector::instance().showNoRecentsYet();
    }
    else if (received_message == "active_dialogs_hide")
    {
        QString aimId = Data::UnserializeActiveDialogHide(&coll_params);
        emit activeDialogHide(aimId);
    }
    else if (received_message == "stickers/meta/get/result")
    {
        Ui::stickers::unserialize(coll_params);
        emit on_stickers();
    }
    else if (received_message == "themes/meta/get/result")
    {
        Ui::themes::unserialize(coll_params);
        get_qt_theme_settings()->flushThemesToLoad();   // here we call delayed themes requests
        emit on_themes_meta();
    }
    else if (received_message == "themes/meta/get/error")
    {
        emit on_themes_meta_error();
    }
    else if (received_message == "stickers/sticker/get/result")
    {
        Ui::stickers::set_sticker_data(coll_params);

        emit on_sticker(
            coll_params.get_value_as_int("set_id"),
            coll_params.get_value_as_int("sticker_id"));
    }
    else if (received_message == "themes/theme/get/result")
    {
        bool failed = coll_params.get_value_as_int("failed", 0) != 0;
        if (!failed)
        {
            Ui::themes::set_theme_data(coll_params);
        }
        emit on_theme(coll_params.get_value_as_int("theme_id"), failed);
    }
    else if (received_message == "chats/info/get/result")
    {
        auto info = std::make_shared<Data::ChatInfo>();
        Data::UnserializeChatInfo(&coll_params, *info);
        if (!info->AimId_.isEmpty())
            emit chatInfo(seq, info);
    }
    else if (received_message == "chats/blocked/result")
    {
        QList<Data::ChatMemberInfo> blocked;
        Data::UnserializeChatMembers(&coll_params, blocked);
        emit chatBlocked(blocked);
    }
    else if (received_message == "chats/info/get/failed")
    {
        auto error_code = coll_params.get_value_as_int("error");
        emit chatInfoFailed(seq, (core::group_chat_info_errors)error_code);
    }
    else if (received_message == "files/download/result")
    {
        fileSharingDownloadResult(seq, coll_params);
    }
    else if (received_message == "image/download/result")
    {
        imageDownloadResult(seq, coll_params);
    }
    else if (received_message == "files/upload/progress")
    {
        fileUploadingProgress(coll_params);
    }
    else if (received_message == "files/upload/result")
    {
        fileUploadingResult(coll_params);
    }
    else if (received_message == "files/speech_to_text/result")
    {
        int error = coll_params.get_value_as_int("error");
        int comeback = coll_params.get_value_as_int("comeback");
        QString text = coll_params.get_value_as_string("text");
        speechToText(seq, error, text, comeback);
    }
    else if (received_message == "contacts/remove/result")
    {
        QString contact = coll_params.get_value_as_string("contact");
        emit contactRemoved(contact);
    }
    else if (received_message == "app_config")
    {
        Ui::AppConfigUptr config(new AppConfig(coll_params));
        Ui::SetAppConfig(config);
    }
    else if (received_message == "my_info")
    {
        Ui::MyInfo()->unserialize(&coll_params);
        Ui::MyInfo()->CheckForUpdate();
        emit myInfo();
    }
    else if (received_message == "signed_url")
    {
        emit signedUrl(coll_params.get_value_as_string("url"));
    }
    else if (received_message == "feedback/sent")
    {
        emit feedbackSent(coll_params.get_value_as_bool("succeeded"));
    }
    else if (received_message == "messages/received/senders")
    {
        QString aimId = coll_params.get_value_as_string("aimid");
        QVector< QString > sendersAimIds;
        if (coll_params.is_value_exist("senders"))
        {
            auto array = coll_params.get_value_as_array("senders");
            for (int i = 0; array && i < array->size(); ++i)
            {
                sendersAimIds.push_back(array->get_at(i)->get_as_string());
            }
        }
        emit messagesReceived(aimId, sendersAimIds);
    }
    else if (received_message == "typing")
    {
        onEventTyping(coll_params, true);
    }
    else if (received_message == "typing/stop")
    {
        onEventTyping(coll_params, false);
    }
    else if (received_message == "contacts/get_ignore/result")
    {
        QVector< QString > ignored_aimids;
        auto array = coll_params.get_value_as_array("aimids");
        for (int i = 0; array && i < array->size(); ++i)
        {
            ignored_aimids.push_back(array->get_at(i)->get_as_string());
        }
        Logic::UpdateIgnoredModel(ignored_aimids);
        emit recv_permit_deny(ignored_aimids.isEmpty());
    }
    else if (received_message == "login_result_attach_uin")
    {
        bool result = coll_params.get_value_as_bool("result");
        int error = result ? 0 : coll_params.get_value_as_int("error");
        emit loginResultAttachUin(seq, error);
    }
    else if (received_message == "login_result_attach_phone")
    {
        bool result = coll_params.get_value_as_bool("result");
        int error = result ? 0 : coll_params.get_value_as_int("error");
        emit loginResultAttachPhone(seq, error);
    }
    else if (received_message == "recv_flags")
    {
        emit recvFlags(coll_params.get_value_as_int("flags"));
    }
    else if (received_message == "update_profile/result")
    {
        emit updateProfile(coll_params.get_value_as_int("error"));
    }
    else if (received_message == "chats/home/get/result")
    {
        bool restart = false, finished = false;
        QString newTag;
        QList<Data::ChatInfo> chats;
        UnserializeChatHome(&coll_params, chats, newTag, restart, finished);
        emit chatsHome(chats, newTag, restart, finished);
    }
    else if (received_message == "chats/home/get/failed")
    {
        int error = coll_params.get_value_as_int("error");
        emit chatsHomeError(error);
	}
    else if (received_message == "user_proxy/result")
    {
        Utils::ProxySettings user_proxy;

        user_proxy.type = (core::proxy_types)coll_params.get_value_as_int("settings_proxy_type", (int32_t)core::proxy_types::auto_proxy);
        user_proxy.proxy_server = coll_params.get_value_as_string("settings_proxy_server", "");
        user_proxy.port = coll_params.get_value_as_int("settings_proxy_port", Utils::ProxySettings::invalid_port);
        user_proxy.username = coll_params.get_value_as_string("settings_proxy_username", "");
        user_proxy.password = coll_params.get_value_as_string("settings_proxy_password", "");
        user_proxy.need_auth = coll_params.get_value_as_bool("settings_proxy_need_auth", false);

        *Utils::get_proxy_settings() = user_proxy;

        emit getUserProxy();
    }
    else if (received_message == "open_created_chat")
    {
        auto aimid = coll_params.get_value_as_string("aimId");
        emit openChat(aimid);
    }
    else if (received_message == "login_new_user")
    {
        emit login_new_user();
    }
    else if (received_message == "set_avatar/result")
    {
        emit Utils::InterConnector::instance().setAvatar(coll_params.get_value_as_int("error"));
    }
    else if (received_message == "chats/role/set/result")
    {
        emit set_chat_role_result(coll_params.get_value_as_int("error"));
    }
    else if (received_message == "chats/block/result")
    {
        emit block_member_result(coll_params.get_value_as_int("error"));
    }
}

bool core_dispatcher::is_im_created() const
{
    return is_im_created_;
}

void core_dispatcher::onEventTyping(core::coll_helper _params, bool _is_typing)
{
    Logic::TypingFires currentTypingStatus(
        _params.get_value_as_string("aimId"),
        _params.get_value_as_string("chatterAimId"),
        _params.get_value_as_string("chatterName"));

    static std::list<Logic::TypingFires> typingFires;

    auto iter_fires = std::find(typingFires.begin(), typingFires.end(), currentTypingStatus);

    if (_is_typing)
    {
        if (iter_fires == typingFires.end())
        {
            typingFires.push_back(currentTypingStatus);
        }
        else
        {
            ++iter_fires->counter_;
        }

        QTimer::singleShot(6000, [this, currentTypingStatus]()
        {
            auto iter_fires = std::find(typingFires.begin(), typingFires.end(), currentTypingStatus);

            if (iter_fires != typingFires.end())
            {
                --iter_fires->counter_;
                if (iter_fires->counter_ <= 0)
                {
                    typingFires.erase(iter_fires);
                    emit typingStatus(currentTypingStatus, false);
                }
            }
        });
    }
    else
    {
        if (iter_fires != typingFires.end())
        {
            --iter_fires->counter_;

            if (iter_fires->counter_ <= 0)
            {
                typingFires.erase(iter_fires);
            }
        }
    }

    emit typingStatus(currentTypingStatus, _is_typing);
}

namespace { std::unique_ptr<core_dispatcher> g_dispatcher; }

core_dispatcher* Ui::GetDispatcher()
{
    if (!g_dispatcher)
    {
        assert(false);
    }

    return g_dispatcher.get();
}

void Ui::create_dispatcher()
{
    if (g_dispatcher)
    {
        assert(false);
    }

    g_dispatcher.reset(new core_dispatcher());
}

void Ui::destroy_dispatcher()
{
    if (!g_dispatcher)
    {
        assert(false);
    }

    g_dispatcher.reset();
}
