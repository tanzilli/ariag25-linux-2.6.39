/*
 * (C) Copyright 2011 ATMEL, Hong Xu
 *
 * PMECC related definitions and routines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

static struct nand_ecclayout pmecc_oobinfo_2048 = {
	.eccbytes = 16,
	.eccpos = { 48, 49, 50, 51, 52, 53, 54, 55,
		    56, 57, 58, 59, 60, 61, 62, 63
		  },
	.oobfree = {
		{2, 46},
	},
};

static int cpu_has_pmecc(void)
{
	return cpu_is_at91sam9x5();
}

static int16_t *pmecc_get_alpha_to(struct atmel_nand_host *host)
{
	int16_t *p;

	if (cpu_is_at91sam9x5()) {
		if (host->sector_size == 512) {
			p = (int16_t *)((u32)host->rom_base +
				PMECC_LOOKUP_TABLE_OFFSET_512);
			return p + PMECC_LOOKUP_TABLE_SIZE_512;
		} else {
			p = (int16_t *)((u32)host->rom_base +
				PMECC_LOOKUP_TABLE_OFFSET_1024);
			return p + PMECC_LOOKUP_TABLE_SIZE_1024;
		}
	}

	return NULL;
}

static int16_t *pmecc_get_index_of(struct atmel_nand_host *host)
{
	int16_t *p = (int16_t *)host->rom_base;

	if (cpu_is_at91sam9x5()) {
		if (host->sector_size == 512)
			p = (int16_t *)((u32)host->rom_base +
				PMECC_LOOKUP_TABLE_OFFSET_512);
		else
			p = (int16_t *)((u32)host->rom_base +
				PMECC_LOOKUP_TABLE_OFFSET_1024);

		return p;
	}

	return NULL;
}

static void pmecc_gen_syndrome(struct mtd_info *mtd, int sector)
{
	int i;
	uint32_t value;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;

	/* Fill odd syndromes */
	for (i = 0; i < host->tt; i++) {
		value = pmecc_readl_rem(host->ecc, sector, i / 2);
		if (i % 2 == 0)
			host->partial_syn[(2 * i) + 1] = value & 0xffff;
		else
			host->partial_syn[(2 * i) + 1] = (value & 0xffff0000)
							  >> 16;
	}
}

static void pmecc_substitute(struct mtd_info *mtd)
{
	int i, j;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;
	int16_t *si;
	int16_t *partial_syn = host->partial_syn;
	int16_t *alpha_to = host->alpha_to;
	int16_t *index_of = host->index_of;

	/* si[] is a table that holds the current syndrome value,
	 * an element of that table belongs to the field
	 */
	si = host->si;

	for (i = 1; i < 2 * NB_ERROR_MAX; i++)
		si[i] = 0;

	/* Computation 2t syndromes based on S(x) */
	/* Odd syndromes */
	for (i = 1; i <= 2 * host->tt - 1; i = i + 2) {
		si[i] = 0;
		for (j = 0; j < host->mm; j++) {
			if (partial_syn[i] & ((unsigned short)0x1 << j))
				si[i] = alpha_to[(i * j)] ^ si[i];
		}
	}
	/* Even syndrome = (Odd syndrome) ** 2 */
	for (i = 2; i <= 2 * host->tt; i = i + 2) {
		j = i / 2;
		if (si[j] == 0)
			si[i] = 0;
		else
			si[i] = alpha_to[(2 * index_of[si[j]]) % host->nn];
	}

	return;
}

static void pmecc_get_sigma(struct mtd_info *mtd)
{
	int i, j, k;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;
	uint32_t dmu_0_count, tmp;
	int16_t *lmu = host->lmu;
	int16_t *si = host->si;
	int16_t tt = host->tt;
	int16_t *index_of = host->index_of;

	/* mu          */
	int mu[NB_ERROR_MAX + 1];

	/* discrepancy */
	int dmu[NB_ERROR_MAX + 1];

	/* delta order   */
	int delta[NB_ERROR_MAX + 1];

	/* index of largest delta */
	int ro;
	int largest;
	int diff;

	dmu_0_count = 0;

	/* First Row */

	/* Mu */
	mu[0] = -1;

	/* Actually -1/2 */
	/* Sigma(x) set to 1 */
	for (i = 0; i < 2 * NB_ERROR_MAX + 1; i++)
		host->smu[0][i] = 0;

	host->smu[0][0] = 1;

	/* discrepancy set to 1 */
	dmu[0] = 1;
	/* polynom order set to 0 */
	lmu[0] = 0;
	/* delta set to -1 */
	delta[0]  = (mu[0] * 2 - lmu[0]) >> 1;

	/* Second Row */

	/* Mu */
	mu[1]  = 0;
	/* Sigma(x) set to 1 */
	for (i = 0; i < (2 * NB_ERROR_MAX + 1); i++)
		host->smu[1][i] = 0;

	host->smu[1][0] = 1;

	/* discrepancy set to S1 */
	dmu[1] = si[1];

	/* polynom order set to 0 */
	lmu[1] = 0;

	/* delta set to 0 */
	delta[1]  = (mu[1] * 2 - lmu[1]) >> 1;

	/* Init the Sigma(x) last row */
	for (i = 0; i < (2 * NB_ERROR_MAX + 1); i++)
		host->smu[tt + 1][i] = 0;

	for (i = 1; i <= tt; i++) {
		mu[i+1] = i << 1;
		/* Begin Computing Sigma (Mu+1) and L(mu) */
		/* check if discrepancy is set to 0 */
		if (dmu[i] == 0) {
			dmu_0_count++;

			if ((tt - (lmu[i] >> 1) - 1) & 0x1)
				tmp = ((tt - (lmu[i] >> 1) - 1) / 2) + 2;
			else
				tmp = ((tt - (lmu[i] >> 1) - 1) / 2) + 1;

			if (dmu_0_count == tmp) {
				for (j = 0; j <= (lmu[i] >> 1) + 1; j++)
					host->smu[tt + 1][j] = host->smu[i][j];

				lmu[tt + 1] = lmu[i];
				return;
			}

			/* copy polynom */
			for (j = 0; j <= lmu[i] >> 1; j++)
				host->smu[i + 1][j] = host->smu[i][j];

			/* copy previous polynom order to the next */
			lmu[i + 1] = lmu[i];
		} else {
			ro = 0;
			largest = -1;
			/* find largest delta with dmu != 0 */
			for (j = 0; j < i; j++) {
				if ((dmu[j]) && (delta[j] > largest)) {
					largest = delta[j];
					ro = j;
				}
			}

			/* compute difference */
			diff = (mu[i] - mu[ro]);

			/* Compute degree of the new smu polynomial */
			if ((lmu[i] >> 1) > ((lmu[ro] >> 1) + diff))
				lmu[i + 1] = lmu[i];
			else
				lmu[i + 1] = ((lmu[ro] >> 1) + diff) * 2;

			/* Init smu[i+1] with 0 */
			for (k = 0; k < (2 * NB_ERROR_MAX + 1); k++)
				host->smu[i+1][k] = 0;

			/* Compute smu[i+1] */
			for (k = 0; k <= lmu[ro] >> 1; k++) {
				if (!(host->smu[ro][k] && dmu[i]))
					continue;

				tmp = host->index_of[dmu[i]] + (host->nn -
				       host->index_of[dmu[ro]]) +
				      host->index_of[host->smu[ro][k]];
				host->smu[i + 1][k + diff] =
					host->alpha_to[tmp % host->nn];
			}

			for (k = 0; k <= lmu[i] >> 1; k++)
				host->smu[i + 1][k] ^= host->smu[i][k];
		}

		/* End Computing Sigma (Mu+1) and L(mu) */
		/* In either case compute delta */
		delta[i + 1]  = (mu[i + 1] * 2 - lmu[i + 1]) >> 1;

		/* Do not compute discrepancy for the last iteration */
		if (i >= tt)
			continue;

		for (k = 0 ; k <= (lmu[i + 1] >> 1); k++) {
			tmp = 2 * (i - 1);
			if (k == 0)
				dmu[i + 1] = si[tmp + 3];
			else if (host->smu[i+1][k] && si[tmp + 3 - k]) {
				tmp = index_of[host->smu[i + 1][k]] +
				      index_of[si[2 * (i - 1) + 3 - k]];
				tmp %= host->nn;
				dmu[i + 1] = host->alpha_to[tmp] ^ dmu[i + 1];
			}
		}
	}

	return;
}


static int pmecc_err_location(struct mtd_info *mtd)
{
	int i;
	/* number of error */
	int err_nbr;
	/* number of roots */
	int roots_nbr;
	int gf_dimension;
	uint32_t val;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;

	if (host->sector_size == 512)
		gf_dimension = GF_DIMENSION_13;
	else
		gf_dimension = GF_DIMENSION_14;

	/* Disable PMECC Error Location IP */
	pmerrloc_writel(host->pmerrloc_base, ELDIS, 0xffffffff);
	err_nbr = 0;

	for (i = 0; i <= host->lmu[host->tt + 1] >> 1; i++) {
		pmerrloc_writel_sigma(host->pmerrloc_base, i,
				      host->smu[host->tt + 1][i]);
		err_nbr++;
	}

	val = pmerrloc_readl(host->pmerrloc_base, ELCFG);
	val |= ((err_nbr - 1) << 16);
	pmerrloc_writel(host->pmerrloc_base, ELCFG, val);

	pmerrloc_writel(host->pmerrloc_base, ELEN,
			host->sector_size * 8 + gf_dimension * host->tt);

	while (!(pmerrloc_readl(host->pmerrloc_base, ELISR)
		 & PMERRLOC_CALC_DONE))
		cpu_relax();

	roots_nbr = (pmerrloc_readl(host->pmerrloc_base, ELISR)
		      & PMERRLOC_ERR_NUM_MASK) >> 8;

	/* Number of roots == degree of smu hence <= tt */
	if (roots_nbr == host->lmu[host->tt + 1] >> 1)
		return err_nbr - 1;

	/* Number of roots does not match the degree of smu
	 * unable to correct error */
	return -1;
}

static void pmecc_correct_data(struct mtd_info *mtd, uint8_t *buf,
		int extra_bytes, int err_nbr)
{
	int i = 0;
	int byte_pos, bit_pos;
	int sector_size, ecc_size;
	uint32_t tmp;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;

	sector_size = host->sector_size;
	/* Get number of ECC bytes */
	ecc_size = nand_chip->ecc.bytes;

	while (err_nbr) {
		byte_pos = (pmerrloc_readl_el(host->pmerrloc_base, i) - 1) / 8;
		bit_pos = (pmerrloc_readl_el(host->pmerrloc_base, i) - 1) % 8;
		dev_dbg(host->dev, "bad : %02x: byte_pos: %d, bit_pos: %d\n",
			*(buf + byte_pos), byte_pos, bit_pos);

		if (byte_pos < (sector_size + extra_bytes)) {
			tmp = sector_size + pmecc_readl(host->ecc, SADDR);
			if (byte_pos < tmp) {
				if (*(buf + byte_pos) & (1 << bit_pos))
					*(buf + byte_pos) &=
						(0xFF ^ (1 << bit_pos));
				else
					*(buf + byte_pos) |= (1 << bit_pos);
			} else {
				if (*(buf + byte_pos + ecc_size) &
				     (1 << bit_pos))
					*(buf + byte_pos + ecc_size) &=
						(0xFF ^ (1 << bit_pos));
				else
					*(buf + byte_pos + ecc_size) |=
						(1 << bit_pos);
			}
		}
		dev_dbg(host->dev, "corr: %02x\n", *(buf + byte_pos));
		i++;
		err_nbr--;
	}

	return;
}

static int pmecc_correction(struct mtd_info *mtd, u32 pmecc_stat, uint8_t *buf,
	u8 *ecc)
{
	int i, err_nbr;
	uint8_t *buf_pos;
	struct nand_chip *nand_chip = mtd->priv;
	int eccbytes = nand_chip->ecc.bytes;
	struct atmel_nand_host *host = nand_chip->priv;

	for (i = 0; i < eccbytes; i++)
		if (ecc[i] != 0xff)
			break;
	/* Erased page, return OK */
	if (i == eccbytes)
		return 0;

	pmerrloc_writel(host->pmerrloc_base, ELCFG,
			(host->sector_size == 512) ? 0 : 1);

	i = 0;
	while (i < host->sector_number) {
		err_nbr = 0;
		if (pmecc_stat & 0x1) {
			buf_pos = buf + i * host->sector_size;

			pmecc_gen_syndrome(mtd, i);
			pmecc_substitute(mtd);
			pmecc_get_sigma(mtd);

			err_nbr = pmecc_err_location(mtd);
			if (err_nbr == -1) {
				dev_err(host->dev, "Too many error.\n");
				mtd->ecc_stats.failed++;
				return -EFAULT;
			} else {
				dev_dbg(host->dev,  "Correct bits...\n");
				pmecc_correct_data(mtd, buf_pos, 0, err_nbr);
				mtd->ecc_stats.corrected += err_nbr;
			}
		}
		i++;
		pmecc_stat >>= 1;
	}

	return 0;
}

static int atmel_nand_pmecc_read_page(struct mtd_info *mtd,
		struct nand_chip *chip, uint8_t *buf, int32_t page)
{
	struct atmel_nand_host *host = chip->priv;
	int eccsize = chip->ecc.size;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	int err = 0, stat;
	int timeout = 10;
	uint8_t *oob = chip->oob_poi;

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_RST);
	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_DISABLE);
	pmecc_writel(host->ecc, CFG, (pmecc_readl(host->ecc, CFG)
		     & ~PMECC_CFG_WRITE_OP) | PMECC_CFG_AUTO_ENABLE);

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_ENABLE);

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_DATA);

	chip->read_buf(mtd, buf, eccsize);
	chip->read_buf(mtd, oob, mtd->oobsize);

	while ((pmecc_readl(host->ecc, SR) & PMECC_SR_BUSY) && (timeout-- > 0))
		cpu_relax();

	stat = pmecc_readl(host->ecc, ISR);

	if (stat != 0) {
		if (pmecc_correction(mtd, stat, buf, &oob[eccpos[0]]))
			err = -1;
	}

	return err;
}

static void atmel_nand_pmecc_write_page(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf)
{
	int i, j;
	int timeout = 10;
	struct atmel_nand_host *host = chip->priv;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_RST);
	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_DISABLE);

	pmecc_writel(host->ecc, CFG, (pmecc_readl(host->ecc, CFG) |
		PMECC_CFG_WRITE_OP) & ~PMECC_CFG_AUTO_ENABLE);

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_ENABLE);
	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_DATA);

	chip->write_buf(mtd, (u8 *)buf, mtd->writesize);

	while ((pmecc_readl(host->ecc, SR) & PMECC_SR_BUSY) && (timeout-- > 0))
		cpu_relax();

	for (i = 0; i < host->sector_number; i++) {
		for (j = 0; j < host->ecc_bytes_per_sector; j++) {
			int pos;

			pos = i * host->ecc_bytes_per_sector + j;
			chip->oob_poi[eccpos[pos]] =
				pmecc_readb_ecc(host->ecc, i, j);
		}
	}
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);

	return;
}

static void atmel_init_pmecc(struct mtd_info *mtd)
{
	uint32_t val;
	struct nand_chip *nand_chip = mtd->priv;
	struct atmel_nand_host *host = nand_chip->priv;
	struct nand_ecclayout *ecc_layout;

	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_RST);
	pmecc_writel(host->ecc, CTRL, PMECC_CTRL_DISABLE);

	switch (host->tt) {
	case 2:
		val = PMECC_CFG_BCH_ERR2;
		break;
	case 4:
		val = PMECC_CFG_BCH_ERR4;
		break;
	case 8:
		val = PMECC_CFG_BCH_ERR8;
		break;
	case 12:
		val = PMECC_CFG_BCH_ERR12;
		break;
	case 24:
		val = PMECC_CFG_BCH_ERR24;
		break;
	}

	if (host->sector_size == 512)
		val |= PMECC_CFG_SECTOR512;
	else if (host->sector_size == 1024)
		val |= PMECC_CFG_SECTOR1024;

	switch (host->sector_number) {
	case 1:
		val |= PMECC_CFG_PAGE_1SECTOR;
		break;
	case 2:
		val |= PMECC_CFG_PAGE_2SECTORS;
		break;
	case 4:
		val |= PMECC_CFG_PAGE_4SECTORS;
		break;
	case 8:
		val |= PMECC_CFG_PAGE_8SECTORS;
		break;
	}

	val |= PMECC_CFG_READ_OP | PMECC_CFG_SPARE_DISABLE
		| PMECC_CFG_AUTO_DISABLE;
	pmecc_writel(host->ecc, CFG, val);

	ecc_layout = nand_chip->ecc.layout;
	pmecc_writel(host->ecc, SAREA, mtd->oobsize - 1);
	pmecc_writel(host->ecc, SADDR, ecc_layout->eccpos[0]);
	pmecc_writel(host->ecc, EADDR,
			ecc_layout->eccpos[ecc_layout->eccbytes - 1]);
	pmecc_writel(host->ecc, CLK, PMECC_CLK_133MHZ);
	pmecc_writel(host->ecc, IDR, 0xff);

	val = pmecc_readl(host->ecc, CTRL);
	val |= PMECC_CTRL_ENABLE;
	pmecc_writel(host->ecc, CTRL, val);
}

static int __init atmel_pmecc_init_params(struct platform_device *pdev,
					 struct atmel_nand_host *host)
{
	struct resource *regs;
	struct resource *regs_pmerr, *regs_rom;
	struct nand_chip *nand_chip;
	struct mtd_info *mtd;
	int res;

	printk(KERN_ERR "atmel_pmecc_init_params\n");

	nand_chip = &host->nand_chip;
	mtd = &host->mtd;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!regs && hard_ecc) {
		dev_warn(host->dev, "Can't get I/O resource regs\nFalling "
				"back on software ECC\n");
	}

	nand_chip->ecc.mode = NAND_ECC_SOFT;	/* enable ECC */
	if (no_ecc)
		nand_chip->ecc.mode = NAND_ECC_NONE;
	if (hard_ecc && regs) {
		host->ecc = ioremap(regs->start, regs->end - regs->start + 1);
		if (host->ecc == NULL) {
			printk(KERN_ERR "atmel_nand: ioremap failed\n");
			res = -EIO;
			goto err_pmecc_ioremap;
		}

		regs_pmerr = platform_get_resource(pdev, IORESOURCE_MEM,
						   2);
		regs_rom = platform_get_resource(pdev, IORESOURCE_MEM,
						 3);
		if (regs_pmerr && regs_rom) {
			host->pmerrloc_base = ioremap(regs_pmerr->start,
			regs_pmerr->end - regs_pmerr->start + 1);
			host->rom_base = ioremap(regs_rom->start,
				regs_rom->end - regs_rom->start + 1);

			if (host->pmerrloc_base && host->rom_base) {
				nand_chip->ecc.mode = NAND_ECC_HW;
				nand_chip->ecc.read_page =
					atmel_nand_pmecc_read_page;
				nand_chip->ecc.write_page =
					atmel_nand_pmecc_write_page;
			} else {
				dev_err(host->dev, "Can not get I/O resource"
				" for HW PMECC controller!\n");
				goto err_pmloc_remap;
			}
		}

		if (nand_chip->ecc.mode != NAND_ECC_HW)
			printk(KERN_ERR "atmel_nand: Can not get I/O resource"
				" for HW ECC Rolling back to software ECC\n");
	}

	if (nand_chip->ecc.mode == NAND_ECC_HW) {
		/* ECC is calculated for the whole page (1 step) */
		nand_chip->ecc.size = mtd->writesize;

		/* set ECC page size and oob layout */
		switch (mtd->writesize) {
		case 2048:
			nand_chip->ecc.bytes = 16;
			nand_chip->ecc.steps = 1;
			nand_chip->ecc.layout = &pmecc_oobinfo_2048;
			host->mm = GF_DIMENSION_13;
			host->nn = (1 << host->mm) - 1;
			/* 2-bits correction */
			host->tt = 2;
			host->sector_size = 512;
			host->sector_number = mtd->writesize /
					      host->sector_size;
			host->ecc_bytes_per_sector = 4;
			host->alpha_to = pmecc_get_alpha_to(host);
			host->index_of = pmecc_get_index_of(host);
			break;
		case 512:
		case 1024:
		case 4096:
			/* TODO */
			dev_warn(host->dev, "Only 2048 page size is currently"
				"supported, Rolling back to software ECC\n");
		default:
			/* page size not handled by HW ECC */
			/* switching back to soft ECC */
			nand_chip->ecc.mode = NAND_ECC_SOFT;
			nand_chip->ecc.calculate = NULL;
			nand_chip->ecc.correct = NULL;
			nand_chip->ecc.hwctl = NULL;
			nand_chip->ecc.read_page = NULL;
			nand_chip->ecc.postpad = 0;
			nand_chip->ecc.prepad = 0;
			nand_chip->ecc.bytes = 0;
			break;
		}
	}

	/* Initialize PMECC core if applicable */
	if ((nand_chip->ecc.mode == NAND_ECC_HW) && cpu_has_pmecc())
		atmel_init_pmecc(mtd);

	return 0;
err_pmloc_remap:
	iounmap(host->ecc);
	if (host->pmerrloc_base)
		iounmap(host->pmerrloc_base);
	if (host->rom_base)
		iounmap(host->rom_base);
err_pmecc_ioremap:
	return -EIO;
}
