#include "Configuration/Config.h"
#include "ScriptMgr.h"
#include "AuctionHouseBot.h"
#include "Mail.h"
#include "BattlegroundMgr.h"
#define auctionbot AuctionHouseBot::instance()
class SeraynAuction : public AuctionHouseScript
{
public:
    SeraynAuction() :AuctionHouseScript("SeraynAuction") {}
    void OnSendAuctionSuccessfulMail(bool& SkipCoreCode, AuctionHouseMgr* me, AuctionEntry* auction, SQLTransaction& trans, Player* owner, uint64 owner_guid, uint32 owner_accId) override
    {
        //sLog->outError("AuctionHouseBot: OnSendAuctionSuccessfulMail");
        if (!SkipCoreCode)
        {
            if (owner || owner_accId)
            {
                uint32 profit = auction->bid + auction->deposit - auction->GetAuctionCut();

                if (owner && owner->GetGUIDLow() != auctionbot->GetAHBplayerGUID())
                {
                    owner->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS, profit);
                    owner->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD, auction->bid);
                    owner->GetSession()->SendAuctionOwnerNotification(auction);
                }

                MailDraft(auction->BuildAuctionMailSubject(AUCTION_SUCCESSFUL), AuctionEntry::BuildAuctionMailBody(auction->bidder, auction->bid, auction->buyout, auction->deposit, auction->GetAuctionCut()))
                    .AddMoney(profit)
                    .SendMailTo(trans, MailReceiver(owner, auction->owner), auction, MAIL_CHECK_MASK_COPIED, sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY));

                if (auction->bid >= 500 * GOLD)
                    if (const GlobalPlayerData* gpd = sWorld->GetGlobalPlayerData(auction->bidder))
                    {
                        uint64 bidder_guid = MAKE_NEW_GUID(auction->bidder, 0, HIGHGUID_PLAYER);
                        Player* bidder = ObjectAccessor::FindPlayerInOrOutOfWorld(bidder_guid);
                        std::string owner_name = "";
                        uint8 owner_level = 0;
                        if (const GlobalPlayerData* gpd_owner = sWorld->GetGlobalPlayerData(auction->owner))
                        {
                            owner_name = gpd_owner->name;
                            owner_level = gpd_owner->level;
                        }
                        CharacterDatabase.PExecute("INSERT INTO log_money VALUES(%u, %u, \"%s\", \"%s\", %u, \"%s\", %u, \"<AH> profit: %ug, bidder: %s %u lvl (guid: %u), seller: %s %u lvl (guid: %u), item %u (%u)\", NOW())", gpd->accountId, auction->bidder, gpd->name.c_str(), bidder ? bidder->GetSession()->GetRemoteAddress().c_str() : "", owner_accId, owner_name.c_str(), auction->bid, (profit / GOLD), gpd->name.c_str(), gpd->level, auction->bidder, owner_name.c_str(), owner_level, auction->owner, auction->item_template, auction->itemCount);
                    }
            }
            SkipCoreCode = true;
        }
        //sLog->outError("AuctionHouseBot: OnSendAuctionSuccessfulMail end");
    }
    void OnSendAuctionExpiredMail(bool& SkipCoreCode, AuctionHouseMgr* me, AuctionEntry* auction, SQLTransaction& trans, Player* owner, uint64 owner_guid, uint32 owner_accId, Item* pItem) override
    {
        //sLog->outError("AuctionHouseBot: OnSendAuctionExpiredMail");
        if (!SkipCoreCode)
        {
            if (owner || owner_accId)
            {
                if (owner && owner->GetGUIDLow() != auctionbot->GetAHBplayerGUID())
                    owner->GetSession()->SendAuctionOwnerNotification(auction);

                MailDraft(auction->BuildAuctionMailSubject(AUCTION_EXPIRED), AuctionEntry::BuildAuctionMailBody(0, 0, auction->buyout, auction->deposit, 0))
                    .AddItem(pItem)
                    .SendMailTo(trans, MailReceiver(owner, auction->owner), auction, MAIL_CHECK_MASK_COPIED, 0);
            }
            else
                sAuctionMgr->RemoveAItem(auction->item_guidlow, true);
            SkipCoreCode = true;
        }
        //sLog->outError("AuctionHouseBot: OnSendAuctionExpiredMail end");
    }
    void OnSendAuctionOutbiddedMail(bool& SkipCoreCode, AuctionHouseMgr* me, AuctionEntry* auction, SQLTransaction& trans, uint32 newPrice, Player* newBidder, uint64 oldBidder_guid, Player* oldBidder) override
    {
        //sLog->outError("AuctionHouseBot: OnSendAuctionOutbiddedMail");
        if (!SkipCoreCode)
        {
            uint32 oldBidder_accId = 0;
            if (!oldBidder)
                oldBidder_accId = sObjectMgr->GetPlayerAccountIdByGUID(oldBidder_guid);

            // old bidder exist
            if (oldBidder || oldBidder_accId)
            {
                // AH_Bot modified here
                if (oldBidder && !newBidder)
                    oldBidder->GetSession()->SendAuctionBidderNotification(auction->GetHouseId(), auction->Id, auctionbot->GetAHBplayerGUID(), newPrice, auction->GetAuctionOutBid(), auction->item_template);
                // modify end
                if (oldBidder && newBidder)
                    oldBidder->GetSession()->SendAuctionBidderNotification(auction->GetHouseId(), auction->Id, newBidder->GetGUID(), newPrice, auction->GetAuctionOutBid(), auction->item_template);

                MailDraft(auction->BuildAuctionMailSubject(AUCTION_OUTBIDDED), AuctionEntry::BuildAuctionMailBody(auction->owner, auction->bid, auction->buyout, auction->deposit, auction->GetAuctionCut()))
                    .AddMoney(auction->bid)
                    .SendMailTo(trans, MailReceiver(oldBidder, auction->bidder), auction, MAIL_CHECK_MASK_COPIED);
            }
            SkipCoreCode = true;
        }
        //sLog->outError("AuctionHouseBot: OnSendAuctionOutbiddedMail end");
    }
    void OnAuctionAdd(AuctionHouseObject* ah, AuctionEntry* entry) override
    {
        //sLog->outError("AuctionHouseBot: OnAuctionAdd");
        auctionbot->IncrementItemCounts(entry);
        //sLog->outError("AuctionHouseBot: OnAuctionAdd end");
    }
    void OnAuctionRemove(AuctionHouseObject* ah, AuctionEntry* entry) override
    {
        //sLog->outError("AuctionHouseBot: OnAuctionRemove");
        auctionbot->DecrementItemCounts(entry, entry->item_template);
        //sLog->outError("AuctionHouseBot: OnAuctionRemove end");
    }
    void OnAuctionHouseUpdate(AuctionHouseMgr* me) override
    {
        //sLog->outError("AuctionHouseBot: Update");
        auctionbot->Update();
        //sLog->outError("AuctionHouseBot: Update end");
    }
};
class SeraynAuctionMail : public MailScript
{
public:
    SeraynAuctionMail() :MailScript("SeraynAuctioniMail"){}
    void OnSendMailTo(bool& SkipCoreCode, MailDraft* me, SQLTransaction& trans, MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked, uint32 deliver_delay, uint32 custom_expiration, Player* pReceiver, Player* pSender, uint32 mailId, MailItemMap& m_items, uint32& m_money, uint32& m_COD) override
    {
        //sLog->outError("AuctionHouseBot: OnSendMailTo");
        if (!SkipCoreCode)
        {
            if (receiver.GetPlayerGUIDLow() == auctionbot->GetAHBplayerGUID())
            {
                if (sender.GetMailMessageType() == MAIL_AUCTION)        // auction mail with items
                        me->deleteIncludedItems(trans, true);
                return;
            }
            time_t deliver_time = time(NULL) + deliver_delay;

            //expire time if COD 3 days, if no COD 30 days, if auction sale pending 1 hour
            uint32 expire_delay;

            // auction mail without any items and money
            if (sender.GetMailMessageType() == MAIL_AUCTION && m_items.empty() && !m_money)
                expire_delay = sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY);
            // mail from battlemaster (rewardmarks) should last only one day
            else if (sender.GetMailMessageType() == MAIL_CREATURE && sBattlegroundMgr->GetBattleMasterBG(sender.GetSenderId()) != BATTLEGROUND_TYPE_NONE)
                expire_delay = DAY;
            // default case: expire time if COD 3 days, if no COD 30 days (or 90 days if sender is a game master)
            else
            {
                if (m_COD)
                    expire_delay = 3 * DAY;
                else if (custom_expiration > 0)
                    expire_delay = custom_expiration * DAY;
                else
                    expire_delay = pSender && pSender->GetSession()->GetSecurity() ? 90 * DAY : 30 * DAY;
            }

            time_t expire_time = deliver_time + expire_delay;

            // Add to DB
            uint8 index = 0;
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_MAIL);
            stmt->setUInt32(index, mailId);
            stmt->setUInt8(++index, uint8(sender.GetMailMessageType()));
            stmt->setInt8(++index, int8(sender.GetStationery()));
            stmt->setUInt16(++index, me->GetMailTemplateId());
            stmt->setUInt32(++index, sender.GetSenderId());
            stmt->setUInt32(++index, receiver.GetPlayerGUIDLow());
            stmt->setString(++index, me->GetSubject());
            stmt->setString(++index, me->GetBody());
            stmt->setBool(++index, !m_items.empty());
            stmt->setUInt64(++index, uint64(expire_time));
            stmt->setUInt64(++index, uint64(deliver_time));
            stmt->setUInt32(++index, m_money);
            stmt->setUInt32(++index, m_COD);
            stmt->setUInt8(++index, uint8(checked));
            trans->Append(stmt);

            for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
            {
                Item* pItem = mailItemIter->second;
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_MAIL_ITEM);
                stmt->setUInt32(0, mailId);
                stmt->setUInt32(1, pItem->GetGUIDLow());
                stmt->setUInt32(2, receiver.GetPlayerGUIDLow());
                trans->Append(stmt);
            }

            // xinef: update global data
            sWorld->UpdateGlobalPlayerMails(receiver.GetPlayerGUIDLow(), 1);

            // For online receiver update in game mail status and data
            if (pReceiver)
            {
                pReceiver->AddNewMailDeliverTime(deliver_time);

                if (pReceiver->IsMailsLoaded())
                {
                    Mail* m = new Mail;
                    m->messageID = mailId;
                    m->mailTemplateId = me->GetMailTemplateId();
                    m->subject = me->GetSubject();
                    m->body = me->GetBody();
                    m->money = me->GetMoney();
                    m->COD = me->GetCOD();

                    for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
                    {
                        Item* item = mailItemIter->second;
                        m->AddItem(item->GetGUIDLow(), item->GetEntry());
                    }

                    m->messageType = sender.GetMailMessageType();
                    m->stationery = sender.GetStationery();
                    m->sender = sender.GetSenderId();
                    m->receiver = receiver.GetPlayerGUIDLow();
                    m->expire_time = expire_time;
                    m->deliver_time = deliver_time;
                    m->checked = checked;
                    m->state = MAIL_STATE_UNCHANGED;

                    pReceiver->AddMail(m);                           // to insert new mail to beginning of maillist

                    if (!m_items.empty())
                    {
                        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
                            pReceiver->AddMItem(mailItemIter->second);
                    }
                }
                else if (!m_items.empty())
                {
                    SQLTransaction temp = SQLTransaction(NULL);
                    me->deleteIncludedItems(temp);
                }
            }
            else if (!m_items.empty())
            {
                SQLTransaction temp = SQLTransaction(NULL);
                me->deleteIncludedItems(temp);
            }
            SkipCoreCode = true;
        }
        //sLog->outError("AuctionHouseBot: OnSendMailTo end");
    }

};

class SeraynAuctionWorld : public WorldScript
{
public:
    SeraynAuctionWorld() : WorldScript("SeraynWorld") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload)
        {
            
            std::string conf_path = _CONF_DIR;
            std::string cfg_file = conf_path + "/mod_ahbot.conf";

            #if PLATFORM == PLATFORM_WINDOWS
            cfg_file = "mod_ahbot.conf";
            #endif
            std::string cfg_def_file = cfg_file + ".dist";

            // Load .conf.dist config
            if (!sConfigMgr->LoadMore(cfg_def_file.c_str()))
            {
            sLog->outString();
            sLog->outError("Module config: Invalid or missing configuration dist file : %s", cfg_def_file.c_str());
            sLog->outError("Module config: Verify that the file exists and has \'[worldserver]' written in the top of the file!");
            sLog->outError("Module config: Use default settings!");
            sLog->outString();
            }

            // Load .conf config
            if (!sConfigMgr->LoadMore(cfg_file.c_str()))
            {
            sLog->outString();
            sLog->outError("Module config: Invalid or missing configuration file : %s", cfg_file.c_str());
            sLog->outError("Module config: Verify that the file exists and has \'[worldserver]' written in the top of the file!");
            sLog->outError("Module config: Use default settings!");
            sLog->outString();
            }
            // Initialize AHBot settings before deleting expired auctions due to AHBot hooks
            auctionbot->InitializeConfiguration();
            //sLog->outString("Initialize AuctionHouseBot Configuration...done!");
        }
    }
    void OnStartup() override
    {
        sLog->outString("Initialize AuctionHouseBot...");
        auctionbot->Initialize();
        //sLog->outString("Initialize AuctionHouseBot...done!");
    }

};

void AddAHBotScripts()
{
    new SeraynAuction();
    new SeraynAuctionMail();
    new SeraynAuctionWorld();
}
