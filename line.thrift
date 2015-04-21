namespace cpp line

enum ContactSetting {
    CONTACT_SETTING_DELETE = 16;
}

enum ContactStatus {
    FRIEND = 1;
    FRIEND_BLOCKED = 2;
    RECOMMEND_BLOCKED = 4;
    DELETED = 5;
    DELETED_BLOCKED = 6;
}

enum ContentType {
    NONE = 0;
    IMAGE = 1;
    VIDEO = 2;
    AUDIO = 3;
    STICKER = 7;
    LOCATION = 15;
}

enum ErrorCode {
    ILLEGAL_ARGUMENT = 0;
    AUTHENTICATION_FAILED = 1;
    DB_FAILED = 2;
    INVALID_STATE = 3;
    EXCESSIVE_ACCESS = 4;
    NOT_FOUND = 5;
    INVALID_LENGTH = 6;
    NOT_AVAILABLE_USER = 7;
    NOT_AUTHORIZED_DEVICE = 8;
    INVALID_MID = 9;
    NOT_A_MEMBER = 10;
    INCOMPATIBLE_APP_VERSION = 11;
    NOT_READY = 12;
    NOT_AVAILABLE_SESSION = 13;
    NOT_AUTHORIZED_SESSION = 14;
    SYSTEM_ERROR = 15;
    NO_AVAILABLE_VERIFICATION_METHOD = 16;
    NOT_AUTHENTICATED = 17;
    INVALID_IDENTITY_CREDENTIAL = 18;
    NOT_AVAILABLE_IDENTITY_IDENTIFIER = 19;
    INTERNAL_ERROR = 20;
    NO_SUCH_IDENTITY_IDENFIER = 21;
    DEACTIVATED_ACCOUNT_BOUND_TO_THIS_IDENTITY = 22;
    ILLEGAL_IDENTITY_CREDENTIAL = 23;
    UNKNOWN_CHANNEL = 24;
    NO_SUCH_MESSAGE_BOX = 25;
    NOT_AVAILABLE_MESSAGE_BOX = 26;
    CHANNEL_DOES_NOT_MATCH = 27;
    NOT_YOUR_MESSAGE = 28;
    MESSAGE_DEFINED_ERROR = 29;
    USER_CANNOT_ACCEPT_PRESENTS = 30;
    USER_NOT_STICKER_OWNER = 32;
    MAINTENANCE_ERROR = 33;
    ACCOUNT_NOT_MATCHED = 34;
    ABUSE_BLOCK = 35;
    NOT_FRIEND = 36;
    NOT_ALLOWED_CALL = 37;
    BLOCK_FRIEND = 38;
    INCOMPATIBLE_VOIP_VERSION = 39;
    INVALID_SNS_ACCESS_TOKEN = 40;
    EXTERNAL_SERVICE_NOT_AVAILABLE = 41;
    NOT_ALLOWED_ADD_CONTACT = 42;
    NOT_CERTIFICATED = 43;
    NOT_ALLOWED_SECONDARY_DEVICE = 44;
    INVALID_PIN_CODE = 45;
    NOT_FOUND_IDENTITY_CREDENTIAL = 46;
    EXCEED_FILE_MAX_SIZE = 47;
    EXCEED_DAILY_QUOTA = 48;
    NOT_SUPPORT_SEND_FILE = 49;
    MUST_UPGRADE = 50;
    NOT_AVAILABLE_PIN_CODE_SESSION = 51;
}

enum IdentityProvider {
    LINE = 1;
}

enum LoginResultType {
    SUCCESS = 1;
    REQUIRE_DEVICE_CONFIRM = 3;
}

enum MIDType {
    USER = 0;
    ROOM = 1;
    GROUP = 2;
}

enum OpType {
    END_OF_OPERATION = 0;
    UPDATE_PROFILE = 1;
    NOTIFIED_UPDATE_PROFILE = 2;
    REGISTER_USERID = 3;
    ADD_CONTACT = 4;
    NOTIFIED_ADD_CONTACT = 5;
    BLOCK_CONTACT = 6;
    UNBLOCK_CONTACT = 7;
    NOTIFIED_RECOMMEND_CONTACT = 8;
    CREATE_GROUP = 9;
    UPDATE_GROUP = 10;
    NOTIFIED_UPDATE_GROUP = 11;
    INVITE_INTO_GROUP = 12;
    NOTIFIED_INVITE_INTO_GROUP = 13;
    LEAVE_GROUP = 14;
    NOTIFIED_LEAVE_GROUP = 15;
    ACCEPT_GROUP_INVITATION = 16;
    NOTIFIED_ACCEPT_GROUP_INVITATION = 17;
    KICKOUT_FROM_GROUP = 18;
    NOTIFIED_KICKOUT_FROM_GROUP = 19;
    CREATE_ROOM = 20;
    INVITE_INTO_ROOM = 21;
    NOTIFIED_INVITE_INTO_ROOM = 22;
    LEAVE_ROOM = 23;
    NOTIFIED_LEAVE_ROOM = 24;
    SEND_MESSAGE = 25;
    RECEIVE_MESSAGE = 26;
    SEND_MESSAGE_RECEIPT = 27;
    RECEIVE_MESSAGE_RECEIPT = 28;
    SEND_CONTENT_RECEIPT = 29;
    RECEIVE_ANNOUNCEMENT = 30;
    CANCEL_INVITATION_GROUP = 31;
    NOTIFIED_CANCEL_INVITATION_GROUP = 32;
    NOTIFIED_UNREGISTER_USER = 33;
    REJECT_GROUP_INVITATION = 34;
    NOTIFIED_REJECT_GROUP_INVITATION = 35;
    UPDATE_SETTINGS = 36;
    NOTIFIED_REGISTER_USER = 37;
    INVITE_VIA_EMAIL = 38;
    NOTIFIED_REQUEST_RECOVERY = 39;
    SEND_CHAT_CHECKED = 40;
    SEND_CHAT_REMOVED = 41;
    NOTIFIED_FORCE_SYNC = 42;
    SEND_CONTENT = 43;
    SEND_MESSAGE_MYHOME = 44;
    NOTIFIED_UPDATE_CONTENT_PREVIEW = 45;
    REMOVE_ALL_MESSAGES = 46;
    NOTIFIED_UPDATE_PURCHASES = 47;
    DUMMY = 48;
    UPDATE_CONTACT = 49;
    NOTIFIED_RECEIVED_CALL = 50;
    CANCEL_CALL = 51;
    NOTIFIED_REDIRECT = 52;
    NOTIFIED_CHANNEL_SYNC = 53;
    FAILED_SEND_MESSAGE = 54;
    NOTIFIED_READ_MESSAGE = 55;
    FAILED_EMAIL_CONFIRMATION = 56;
    NOTIFIED_CHAT_CONTENT = 58;
    NOTIFIED_PUSH_NOTICENTER_ITEM = 59;
}

struct Contact {
    1: string mid;
    11: ContactStatus status;
    22: string displayName;
    26: string statusMessage;
    35: i32 attributes;
    37: string picturePath;
}

struct Group {
    1: string id;
    10: string name;
    20: list<Contact> members;
    21: Contact creator;
    22: list<Contact> invitee;
}

struct Location {
    1: string title;
    2: string address;
    3: double latitude;
    4: double longitude;
}

struct LoginResult {
    1: string authToken;
    2: string certificate;
    3: string verifier;
    4: string pinCode;
    5: LoginResultType type;
}

struct Message {
    1: string from;
    2: string to;
    3: MIDType toType;
    4: string id;
    5: i64 createdTime;
    10: string text;
    11: optional Location location;
    15: ContentType contentType;
    17: binary contentPreview;
    18: map<string, string> contentMetadata;
}

struct MessageBox {
    1: string id;
    9: MIDType midType;
    10: list<Message> lastMessages;
}

struct MessageBoxWrapUp {
    1: MessageBox messageBox;
    3: list<Contact> contacts;
}

struct MessageBoxWrapUpList {
    1: list<MessageBoxWrapUp> messageBoxWrapUpList;
}

struct Operation {
    1: i64 revision;
    2: i64 createdTime;
    3: OpType type;
    4: i32 reqSeq;
    10: string param1;
    11: string param2;
    12: string param3;
    20: Message message;
}

struct Profile {
    1: string mid;
    20: string displayName;
    24: string statusMessage;
    33: string picturePath;
}

struct Room {
    1: string mid;
    10: list<Contact> contacts;
}

exception TalkException {
    1: ErrorCode code;
    2: string reason;
}

service TalkService {
    void acceptGroupInvitation(
        1: i32 reqSeq,
        2: string groupId) throws(1: TalkException e);

    list<Operation> fetchOperations(
        2: i64 localRev,
        3: i32 count) throws(1: TalkException e);

    list<string> getAllContactIds() throws(1: TalkException e);

    Contact getContact(
        2: string id) throws(1: TalkException e);

    list<Contact> getContacts(
        2: list<string> ids) throws(1: TalkException e);

    Group getGroup(
        2: string groupId) throws(1: TalkException e);

    list<string> getGroupIdsInvited() throws(1: TalkException e);

    list<string> getGroupIdsJoined() throws(1: TalkException e);

    list<Group> getGroups(
        2: list<string> groupIds) throws(1: TalkException e);

    i64 getLastOpRevision() throws(1: TalkException e);

    MessageBoxWrapUpList getMessageBoxCompactWrapUpList(
        2: i32 start,
        3: i32 messageBoxCount) throws(1: TalkException e);

    list<Message> getPreviousMessages(
        2: string messageBoxId,
        3: i64 endSeq,
        4: i32 messagesCount) throws(1: TalkException e);

    list<Message> getRecentMessages(
        2: string messageBoxId,
        3: i32 messagesCount) throws(1: TalkException e);

    Room getRoom(
        2: string roomId) throws(1: TalkException e);

    LoginResult loginWithIdentityCredentialForCertificate(
        8: IdentityProvider identityProvider,
        3: string identifier,
        4: string password,
        5: bool keepLoggedIn,
        6: string accessLocation,
        7: string systemName,
        9: string certificate) throws(1: TalkException e);

    LoginResult loginWithVerifierForCertificate(
        3: string verifier) throws(1: TalkException e);

    void leaveGroup(
        1: i32 reqSeq,
        2: string groupId) throws(1: TalkException e);

    void leaveRoom(
        1: i32 reqSeq,
        2: string roomId) throws(1: TalkException e);

    Profile getProfile() throws(1: TalkException e);

    void rejectGroupInvitation(
        1: i32 reqSeq,
        2: string groupId) throws(1: TalkException e);

    Message sendMessage(
        1: i32 seq,
        2: Message message) throws(1: TalkException e);

    void updateContactSetting(
        1: i32 reqSeq,
        2: string mid,
        3: ContactSetting flag,
        4: string value) throws(1: TalkException e);
}
