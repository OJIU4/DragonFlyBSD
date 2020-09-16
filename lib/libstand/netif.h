/*	$NetBSD: netif.h,v 1.4 1995/09/14 23:45:30 pk Exp $	*/

/* $FreeBSD: src/lib/libstand/netif.h,v 1.1.1.1.6.1 2000/09/10 01:24:16 ps Exp $ */

#ifndef _STAND_NETIF_H_
#define	_STAND_NETIF_H_

#include "iodesc.h"

#define NENTS(x)	sizeof(x)/sizeof(x[0])

struct netif_driver {
	const	char *netif_bname;
	int	(*netif_match)(struct netif *, void *);
	int	(*netif_probe)(struct netif *, void *);
	void	(*netif_init)(struct iodesc *, void *);
	int	(*netif_get)(struct iodesc *, void *, size_t, time_t);
	int	(*netif_put)(struct iodesc *, void *, size_t);
	void	(*netif_end)(struct netif *);
	struct	netif_dif *netif_ifs;
	int	netif_nifs;
};

struct netif_dif {
	int		dif_unit;
	int		dif_nsel;
	struct netif_stats *dif_stats;
	void		*dif_private;
	/* the following fields are used internally by the netif layer */
	u_long		dif_used;
};

struct netif_stats {
	int	collisions;
	int	collision_error;
	int	missed;
	int	sent;
	int	received;
	int	deferred;
	int	overflow;
};

struct netif {
	struct netif_driver	*nif_driver;
	int			nif_unit;
	int			nif_sel;
	void			*nif_devdata;
};

extern struct netif_driver	*netif_drivers[];	/* machdep */

extern int			netif_debug;

void		netif_init(void);
struct netif	*netif_select(void *);
int		netif_probe(struct netif *, void *);
void		netif_attach(struct netif *, struct iodesc *, void *);
void		netif_detach(struct netif *);
ssize_t		netif_get(struct iodesc *, void *, size_t, time_t);
ssize_t		netif_put(struct iodesc *, void *, size_t);

int		netif_open(void *);
int		netif_close(int);

struct iodesc	*socktodesc(int);
	
#endif /* !_STAND_NETIF_H_ */

