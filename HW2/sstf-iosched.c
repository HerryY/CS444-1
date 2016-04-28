/*
 * Author:	Robert Taylor
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>

struct sstf_data {
	struct list_head queue;
	sector_t head_position;
	int direction;
};

static void sstf_merged_requests
(struct request_queue *q, struct request *rq, struct request *next)
{
	list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	printk("Start of dispatch\n");

	if(!list_empty(&nd->queue))
	{
		struct request *next_req, *prev_req, *req;
	
		next_req = list_entry(nd->queue.next, struct request, queuelist);
		prev_req = list_entry(nd->queue.prev, struct request, queuelist);

		if(prev_req == next_req){
			printk("First request\n");
			req = next_req;   
		}else{
			printk("Next requests\n");
			if(nd->direction == 1) {
				printk("Forward\n");
			
				if(next_req->__sector > nd->head_position){
					req = next_req;
				}else{
					nd->direction = 0;
					req = prev_req;
				}
			}else{
				printk("Backward\n");

				if(prev_req->__sector < nd->head_position){
					req = prev_req;
				}else{
					nd->direction = 1;
					req = next_req;
				}
			}
		}
 
		list_del_init(&req->queuelist);
		nd->head_position = blk_rq_pos(req) + blk_rq_sectors(req);
		elv_dispatch_add_tail(q, req);

		printk("Accessing LOOK %llu\n",(unsigned long long) req->__sector);
		return 1;
	}
	return 0;
}


static void sstf_add_request(struct request_queue *q, struct request *rq)
{
    struct sstf_data *nd = q->elevator->elevator_data;
    struct request *next_req, *prev_req;
 
    printk("Starting add\n");

    if(list_empty(&nd->queue)){
        printk("Empty List\n");
        list_add(&rq->queuelist, &nd->queue);
    }else{
		next_req = list_entry(nd->queue.next, struct request, queuelist);
		prev_req = list_entry(nd->queue.prev, struct request, queuelist);

		while(blk_rq_pos(rq) > blk_rq_pos(next_req)){
			next_req = list_entry(next_req->queuelist.next, struct request, queuelist);
			prev_req = list_entry(prev_req->queuelist.prev, struct request, queuelist);
		}

		list_add(&rq->queuelist, &prev_req->queuelist);
	}
	printk("SSTF adding %llu\n", (unsigned long long) rq->__sector);
}


static struct request * sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if(rq->queuelist.prev == &nd->queue){
		return NULL;
	}
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request * sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if(rq->queuelist.next == &nd->queue){
		return NULL;
	}
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *nd;
	struct elevator_queue *eq;

    eq = elevator_alloc(q, e);
	if(!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if(!nd){
	kobject_put(&eq->kobj);
	return -ENOMEM;
	}

	nd->head_position = 0;
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn = sstf_merged_requests,
		.elevator_dispatch_fn = sstf_dispatch,
		.elevator_add_req_fn = sstf_add_request,
		.elevator_former_req_fn = sstf_former_request,
		.elevator_latter_req_fn = sstf_latter_request,
		.elevator_init_fn = sstf_init_queue,
		.elevator_exit_fn = sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Robert Taylor");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF I/O scheduler");

