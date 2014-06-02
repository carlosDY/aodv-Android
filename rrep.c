/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "rrep.h"

extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;
extern aodv_route * g_local_route;

void convert_rrep_to_host(rrep * tmp_rrep) {
	tmp_rrep->dst_id = ntohl(tmp_rrep->dst_id);
	tmp_rrep->src_id = ntohl(tmp_rrep->src_id);
	tmp_rrep->path_metric = ntohl(tmp_rrep->path_metric);
}

void convert_rrep_to_network(rrep * tmp_rrep) {
	tmp_rrep->dst_id = htonl(tmp_rrep->dst_id);
	tmp_rrep->src_id = htonl(tmp_rrep->src_id);
	tmp_rrep->path_metric = htonl(tmp_rrep->path_metric);

}

int recv_rrep(task * tmp_packet) {
	aodv_route *send_route;
	aodv_route *recv_route;
	aodv_neigh *tmp_neigh = NULL;
	rrep *tmp_rrep;
	u_int32_t path_metric;
	int iam_destination = 0;
#ifdef DEBUG
	char dst_ip[16];
	char src_ip[16];
#endif
	tmp_rrep = tmp_packet->data;
	convert_rrep_to_host(tmp_rrep);
	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);
#ifdef DEBUG
	printk("Recive RREP from %s\n", inet_ntoa(tmp_packet->src_ip));
#endif
	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk("Ignoring RREP received from unknown neighbor\n");
#endif
		return 1;
	}

	//Update neighbor timelife
		delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
		insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
				* (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
		update_timer_queue();
		tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
				+ getcurrtime();

	tmp_rrep->num_hops++;

#ifdef DEBUG

	strcpy(src_ip, inet_ntoa(tmp_rrep->src_ip));
	strcpy(dst_ip, inet_ntoa(tmp_rrep->dst_ip));

	printk("received new rrep from %s to %s with ToS: %u - last hop: %s\n", dst_ip, src_ip, tmp_rrep->tos, inet_ntoa(tmp_packet->src_ip));
	printk("g_mesh_ip is %s\n", inet_ntoa(g_mesh_ip));
#endif

	if (tmp_rrep->src_ip == g_mesh_ip || (tmp_rrep->src_ip == g_null_ip && tmp_rrep->gateway == g_mesh_ip))
		iam_destination = 1;

	if (iam_destination) { //I'm the source of the flow (the destination of the RREP)
#ifdef DEBUG
		printk("I'm the source of the flow (the destination of the RREP)\n");
#endif
		delete_timer(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos,
					TASK_RESEND_RREQ);

		update_timer_queue();
		path_metric = tmp_rrep->path_metric;
		//Create (or update) the first hop of the route
		rreq_aodv_route(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos, tmp_neigh, tmp_rrep->num_hops,
				tmp_rrep->dst_id, tmp_packet->dev, path_metric);

		send_route = find_aodv_route_by_id(tmp_rrep->dst_ip, tmp_rrep->dst_id);

		if (!send_route) {
#ifdef DEBUG
			printk("No reverse-route for RREP from: %s to: %s with TOS %u\n", dst_ip, src_ip, tmp_rrep->tos);
#endif
			return 0;
		}
		rrep_aodv_route(send_route);
		send_route->last_hop = g_mesh_ip;

		/*****************************************************
            �����ͨ·�����֣����е���Ŀ�ĵĶ�·��Ŀ��
            �������Ӧ��ͨ·������dst����һ������ͨ·����
            ֪ͨDTN�㣬�Լ�ɾ����Ӧ��·��Ŀ����gen_rcvp
            ��ʵ��
		******************************************************/

#ifdef CaiDebug
char s[20];
char d[20];
strcpy(s,inet_ntoa(tmp_rrep->src_ip));
strcpy(d,inet_ntoa(tmp_rrep->dst_ip));
        printk("The tmp_rrep's src is %s,dst is %s\n",s,d);
#endif
		brk_link *tmp_link;
		tmp_link = find_first_brk_link_with_dst(tmp_rrep->dst_ip);//rreq��Դ��Ϊ·�ɷ��ֵ�Ŀ�ĵأ�ҲΪ��·���Ŀ�ĵ�ַ
		if(tmp_link==NULL)   printk("no brk links to this dst\n");
		else{
			printk("start to gen rcvp\n");
		  	gen_rcvp(tmp_rrep->dst_ip);
		}
		/*****************************************************/

	}

	else {
		recv_route = find_aodv_route_by_id(tmp_rrep->src_ip, tmp_rrep->src_id); //the route created by the RREQ

		if (!recv_route) {
#ifdef DEBUG
			printk("Reverse Route has timed out! - RREP is not forwarded!\n");
#endif
			return 1;
		}

		path_metric = tmp_rrep->path_metric - recv_route->path_metric;
		//Create (or update) the route from source to destination
		rreq_aodv_route(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos,
				tmp_neigh, tmp_rrep->num_hops,
				tmp_rrep->dst_id, tmp_packet->dev, path_metric);
		send_route = find_aodv_route_by_id(tmp_rrep->dst_ip, tmp_rrep->dst_id);
#ifdef DEBUG
		printk("recv_rrep: send_route %s to: %s with NEXT %s\n", inet_ntoa(send_route->src_ip), inet_ntoa(send_route->dst_ip), inet_ntoa(send_route->next_hop));
#endif
		if (!send_route) {
#ifdef DEBUG

			printk("No reverse-route for RREP from: %s to: %s with TOS %u\n", dst_ip, src_ip, tmp_rrep->tos);
			printk("Not Forwarding RREP!\n");
#endif
			return 0;
		}
		rrep_aodv_route(recv_route);
		rrep_aodv_route(send_route);
		send_route->last_hop = recv_route->next_hop;
		recv_route->last_hop = send_route->next_hop;

		convert_rrep_to_network(tmp_rrep);
		send_message(recv_route->next_hop, NET_DIAMETER, tmp_rrep, sizeof(rrep));

	}

	return 0;
}

int gen_rrep(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos) {
	aodv_route *src_route;
	rrep *tmp_rrep;
#ifdef DEBUG
	char src[16];
	char dst[16];
	strcpy (src, inet_ntoa(src_ip));
	strcpy (dst, inet_ntoa(dst_ip));
	printk("I'm generating a new rrep, from %s to %s\n",src, dst);
#endif
	if ((tmp_rrep = (rrep *) kmalloc(sizeof(rrep), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk("Can't allocate new rrep\n");
#endif
		return 0;
	}

	src_route = find_aodv_route(dst_ip, src_ip, tos);
	/* Get the source and destination IP address from the RREQ */

	if (!src_route) { //symmetric
#ifdef DEBUG
			printk("RREP: No route to Source! src: %s\n", inet_ntoa(src_ip));
#endif
			return 1;
	}


	if ((tmp_rrep = (rrep *) kmalloc(sizeof(rrep), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk("Can't allocate new rrep\n");
#endif
		return 0;
	}

	tmp_rrep->type = RREP_MESSAGE;
	tmp_rrep->a = 0;
	tmp_rrep->reserved1 = 0;
	tmp_rrep->src_ip = src_route->dst_ip;
	tmp_rrep->dst_ip = dst_ip;
	g_local_route->dst_id = g_local_route->dst_id + 1;
	tmp_rrep->dst_id = g_local_route->dst_id;
	tmp_rrep->num_hops = 0;
	tmp_rrep->src_id = src_route->dst_id;
	tmp_rrep->tos = tos;
	tmp_rrep->q=0;
	tmp_rrep->path_metric = src_route->path_metric;

	//Update the route to REPLIED state
	rrep_aodv_route(src_route);
	src_route->last_hop = dst_ip;

	convert_rrep_to_network(tmp_rrep);
	send_message(src_route->next_hop, NET_DIAMETER, tmp_rrep, sizeof(rrep));

	kfree(tmp_rrep);
	return 1;

}

