// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the body of custom script commands in this file.
 * Format:
 *	BUILDIN_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//BUILDIN_FUNC(example)
//{
//	ShowInfo("buildin_example: It works!\n");
//	script_pushint(st,1);
//	return 0;
//}

/**
 * Make a player add an item to his storage
 * @param sd : player
 * @param stor : Storage data
 * @param item_data : item to add
 * @param amount : quantity of items
 * @return 0:success, 1:failed, 2:failed because of room or stack checks
 */
int storage_getitem(struct map_session_data* sd, struct s_storage *stor, struct item *it, int amount)
{
	struct item_data *data;
	int i;

	if( it->nameid == 0 || amount <= 0 )
		return 1;

	data = itemdb_search(it->nameid);

	if( data->stack.storage && amount > data->stack.amount ) // item stack limitation
		return 2;

	if( !itemdb_canstore(it, pc_get_group_level(sd)) ) { // Check if item is storable. [Skotlex]
		return 1;
	}

	if( itemdb_isstackable2(data) ) { // Stackable
		for( i = 0; i < stor->max_amount; i++ ) {
			if( compare_item(&stor->u.items_storage[i], it) ) { // existing items found, stack them
				if( amount > MAX_AMOUNT - stor->u.items_storage[i].amount || ( data->stack.storage && amount > data->stack.amount - stor->u.items_storage[i].amount ) )
					return 2;

				stor->u.items_storage[i].amount += amount;
				if(sd->state.storage_flag){
					stor->dirty = true;
					clif_storageitemadded(sd,&stor->u.items_storage[i],i,amount);
				}
				return 0;
			}
		}
	}

	if( stor->amount >= stor->max_amount )
		return 2;

	// find free slot
	ARR_FIND( 0, stor->max_amount, i, stor->u.items_storage[i].nameid == 0 );
	if( i >= stor->max_amount )
		return 2;

	// add item to slot
	memcpy(&stor->u.items_storage[i],it,sizeof(stor->u.items_storage[0]));
	stor->amount++;
	stor->u.items_storage[i].amount = amount;
	if(sd->state.storage_flag){
		stor->dirty = true;
		clif_storageitemadded(sd,&stor->u.items_storage[i],i,amount);
		clif_updatestorageamount(sd, stor->amount, stor->max_amount);
	}
	
	return 0;
}

/*==========================================
 * getstorageitem <item id>,<amount>{,<account ID>};
 * getstorageitem "<item name>",<amount>{,<account ID>};
 *------------------------------------------*/
BUILDIN_FUNC(getstorageitem)
{
	int get_count, i;
	t_itemid nameid;
	unsigned short amount;
	struct item it;
	TBL_PC *sd;
	unsigned char flag = 0;
	const char* command = script_getfuncname(st);
	struct item_data *id = NULL;

	if( script_isstring(st, 2) ) {// "<item name>"
		const char *name = script_getstr(st, 2);

		id = itemdb_searchname(name);
		if( id == NULL ){
			ShowError("buildin_getstorageitem: Nonexistant item %s requested.\n", name);
			return SCRIPT_CMD_FAILURE; //No item created.
		}
		nameid = id->nameid;
	} else {// <item id>
		nameid = script_getnum(st, 2);
		if( !(id = itemdb_exists(nameid)) ){
			ShowError("buildin_getstorageitem: Nonexistant item %u requested.\n", nameid);
			return SCRIPT_CMD_FAILURE; //No item created.
		}
	}

	std::shared_ptr<s_pet_db> pet = pet_db_search(nameid, PET_EGG);
	
	if (pet){ //pet egg not available here.
		script_pushint(st,1);
		return SCRIPT_CMD_SUCCESS;
	}
	
	// <amount>
	if( (amount = script_getnum(st,3)) <= 0)
		return SCRIPT_CMD_SUCCESS; //return if amount <=0, skip the useles iteration

	memset(&it,0,sizeof(it));
	it.nameid = nameid;
	it.identify = 1;
	it.bound = BOUND_NONE;

	script_mapid2sd(4,sd);

	if( sd == NULL ) // no target
		return SCRIPT_CMD_SUCCESS;

	//Check if it's stackable.
	if (!itemdb_isstackable2(id))
		get_count = 1;
	else
		get_count = amount;
	
	for (i = 0; i < amount; i += get_count)
	{
		// if not pet egg
		if(flag = storage_getitem(sd, &sd->storage, &it, get_count)){
			script_pushint(st,flag);
			return SCRIPT_CMD_SUCCESS;
		}
	}
	return SCRIPT_CMD_SUCCESS;
}

/*==========================================
 * getstorageitem2 <item id>,<amount>,<identify>,<refine>,<attribute>,<card1>,<card2>,<card3>,<card4>{,<account ID>};
 * getstorageitem2 "<item name>",<amount>,<identify>,<refine>,<attribute>,<card1>,<card2>,<card3>,<card4>{,<account ID>};
 *
 * getstorageitem3 <item id>,<amount>,<identify>,<refine>,<attribute>,<card1>,<card2>,<card3>,<card4>,<RandomIDArray>,<RandomValueArray>,<RandomParamArray>{,<account ID>};
 * getstorageitem3 "<item name>",<amount>,<identify>,<refine>,<attribute>,<card1>,<card2>,<card3>,<card4>,<RandomIDArray>,<RandomValueArray>,<RandomParamArray>{,<account ID>};
 *------------------------------------------*/
BUILDIN_FUNC(getstorageitem2)
{
	t_itemid nameid;
	unsigned short amount;
	int iden, ref, attr;
	t_itemid c1, c2, c3, c4;
	char bound = BOUND_NONE;
	struct item_data *item_data = NULL;
	struct item item_tmp;
	unsigned char flag = 0;
	TBL_PC *sd;
	const char* command = script_getfuncname(st);
	int offset = 0;
	int aid_pos = 11;
	
	if (strcmpi(command,"getstorageitem3") == 0) {
		offset = 11;
		aid_pos = 14;
	} 
	
	script_mapid2sd(aid_pos,sd);

	if( sd == NULL ) // no target
		return SCRIPT_CMD_SUCCESS;

	if( script_isstring(st, 2) ) {
		const char *name = script_getstr(st, 2);

		if( (item_data = itemdb_searchname(name)) == NULL ){
			ShowError("buildin_getstorageitem2: Nonexistant item %s requested (by conv_str).\n", name);
			return SCRIPT_CMD_FAILURE; //No item created.
		}
		nameid = item_data->nameid;
	} else {
		nameid = script_getnum(st, 2);
		if( (item_data = itemdb_exists(nameid)) == NULL ){
			ShowError("buildin_getstorageitem2: Nonexistant item %u requested (by conv_num).\n", nameid);
			return SCRIPT_CMD_FAILURE; //No item created.
		}
	}

	std::shared_ptr<s_pet_db> pet = pet_db_search(nameid, PET_EGG);
	
	if (pet){ //pet egg not available here.
		script_pushint(st,1);
		return SCRIPT_CMD_SUCCESS;
	}
	
	amount = script_getnum(st,3);
	iden = script_getnum(st,4);
	ref = script_getnum(st,5);
	attr = script_getnum(st,6);
	c1 = script_getnum(st,7);
	c2 = script_getnum(st,8);
	c3 = script_getnum(st,9);
	c4 = script_getnum(st,10);

	if( item_data ) {
		int get_count = 0, i;
		memset(&item_tmp,0,sizeof(item_tmp));
		if( item_data->type == IT_WEAPON || item_data->type == IT_ARMOR || item_data->type == IT_SHADOWGEAR ) {
			if(ref > MAX_REFINE)
				ref = MAX_REFINE;
		}
		else if( item_data->type == IT_PETEGG ) {
			iden = 1;
			ref = 0;
		}
		else {
			iden = 1;
			ref = attr = 0;
		}

		item_tmp.nameid = nameid;
		item_tmp.identify = iden;
		item_tmp.refine = ref;
		item_tmp.attribute = attr;
		item_tmp.card[0] = c1;
		item_tmp.card[1] = c2;
		item_tmp.card[2] = c3;
		item_tmp.card[3] = c4;
		item_tmp.bound = bound;

		if (offset != 0) {
			int res = script_getitem_randomoption(st, sd, &item_tmp, command, offset);
			if (res == SCRIPT_CMD_FAILURE)
				return SCRIPT_CMD_FAILURE;
		}

		//Check if it's stackable.
		if (!itemdb_isstackable2(item_data))
			get_count = 1;
		else
			get_count = amount;

		for (i = 0; i < amount; i += get_count)
		{
			if(flag = storage_getitem(sd, &sd->storage, &item_tmp, get_count)){
				script_pushint(st,flag);
				return SCRIPT_CMD_SUCCESS;
			}
		}
	}
	return SCRIPT_CMD_SUCCESS;
}